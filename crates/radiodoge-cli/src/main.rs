//! RadioDoge CLI — Rust port of RadioDogeSharp (C#) and serdog (C).
//!
//! Provides a complete headless interface to the Heltec ESP32 LoRa device:
//! send Dogecoin, receive packets, generate wallets, and run as a daemon.
//! No GUI required — works on Windows, Linux, macOS, and eventually Raspberry Pi.
//!
//! Much CLI. Very terminal. Such Rust. Wow. 🐕

use std::sync::Arc;
use std::time::Duration;

use anyhow::{Context, Result};
use clap::{Parser, Subcommand};
use radiodoge_core::{radio, wallet};
use radiodoge_core::serial::SerialManager;
use radiodoge_core::types::{IncomingPacket, NodeAddress};

// ─── CLI Definition ──────────────────────────────────────────────────────────

#[derive(Parser)]
#[command(
    name = "radiodoge-cli",
    version = "0.2.4",
    about = "🐕 RadioDoge CLI — Wireless P2P Dogecoin over LoRa\n\nMuch CLI. Very terminal. Such Rust. Wow.",
    long_about = None,
)]
struct Cli {
    #[command(subcommand)]
    command: Commands,

    /// Enable verbose debug logging
    #[arg(short, long, global = true)]
    verbose: bool,
}

#[derive(Subcommand)]
enum Commands {
    /// List all available serial ports on this system
    ///
    /// Example: radiodoge-cli ports
    Ports,

    /// Wallet operations (generate, validate)
    Wallet {
        #[command(subcommand)]
        cmd: WalletCommands,
    },

    /// Send a Dogecoin transaction over LoRa radio
    ///
    /// Example: radiodoge-cli send -p COM3 -t DH5yaieq... -a 4.20
    Send {
        /// Serial port (e.g. COM3 on Windows, /dev/ttyUSB0 on Linux)
        #[arg(short, long)]
        port: String,

        /// Recipient Dogecoin address (must start with 'D')
        #[arg(short = 't', long = "to")]
        to_address: String,

        /// Amount in DOGE (e.g. 4.20)
        #[arg(short, long)]
        amount: f64,

        /// Optional transaction memo (up to 190 bytes)
        #[arg(short, long)]
        memo: Option<String>,
    },

    /// Listen for incoming LoRa packets and print them
    ///
    /// Example: radiodoge-cli receive -p COM3 --timeout 60
    Receive {
        /// Serial port
        #[arg(short, long)]
        port: String,

        /// Stop after this many seconds (0 = run forever)
        #[arg(short = 'T', long, default_value = "30")]
        timeout: u64,
    },

    /// Ping the Heltec device and report round-trip success/failure
    ///
    /// Example: radiodoge-cli ping -p COM3
    Ping {
        /// Serial port
        #[arg(short, long)]
        port: String,
    },

    /// Interactive REPL — replaces RadioDogeSharp's menu-driven interface
    ///
    /// Type 'help' at the prompt for available commands.
    ///
    /// Example: radiodoge-cli connect COM3
    Connect {
        /// Serial port
        port: String,
    },

    /// Run as a headless daemon — replaces serdog (C serial daemon)
    ///
    /// Connects to the device, logs all packets to stdout, and relays them
    /// indefinitely. Useful for Raspberry Pi gateway deployments.
    ///
    /// Example: radiodoge-cli daemon -p /dev/ttyUSB0
    Daemon {
        /// Serial port
        #[arg(short, long)]
        port: String,
    },
}

#[derive(Subcommand)]
enum WalletCommands {
    /// Generate a new Dogecoin keypair (address + private key)
    ///
    /// ⚠️  Save the private key immediately — it is NOT stored anywhere!
    ///
    /// Example: radiodoge-cli wallet generate
    Generate,

    /// Validate whether a string is a valid Dogecoin address
    ///
    /// Example: radiodoge-cli wallet validate DH5yaieqoZN36fDVciNyRueRGvGLR3mr7L
    Validate {
        /// Dogecoin address to validate
        address: String,
    },
}

// ─── Main ────────────────────────────────────────────────────────────────────

#[tokio::main]
async fn main() -> Result<()> {
    let cli = Cli::parse();

    // Initialise logging
    let log_level = if cli.verbose { "debug" } else { "info" };
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or(log_level))
        .init();

    match cli.command {
        Commands::Ports => cmd_ports(),
        Commands::Wallet { cmd } => cmd_wallet(cmd),
        Commands::Send { port, to_address, amount, memo } => {
            cmd_send(&port, &to_address, amount, memo.as_deref()).await
        }
        Commands::Receive { port, timeout } => cmd_receive(&port, timeout).await,
        Commands::Ping { port } => cmd_ping(&port).await,
        Commands::Connect { port } => cmd_connect(&port).await,
        Commands::Daemon { port } => cmd_daemon(&port).await,
    }
}

// ─── Command Implementations ─────────────────────────────────────────────────

/// List all available serial ports.
fn cmd_ports() -> Result<()> {
    let ports = SerialManager::list_ports();
    if ports.is_empty() {
        println!("No serial ports found. Is the Heltec connected via USB? Very sad. 😢");
    } else {
        println!("🐕 Available serial ports:");
        for p in &ports {
            println!("  • {}", p);
        }
        println!("\nUse one of these with -p / --port");
    }
    Ok(())
}

/// Wallet sub-commands.
fn cmd_wallet(cmd: WalletCommands) -> Result<()> {
    match cmd {
        WalletCommands::Generate => {
            let w = wallet::generate_keypair().context("Failed to generate keypair")?;
            println!("🐕 New Dogecoin Wallet — SAVE THIS PRIVATELY!\n");
            println!("  Address (share this):     {}", w.address);
            println!("  Public Key (hex):          {}", w.public_key_hex);
            println!("  Private Key (WIF, SECRET): {}", w.private_key_wif);
            println!("\n⚠️  The private key is shown ONCE. Write it down. Lose it = lose DOGE.");
        }
        WalletCommands::Validate { address } => {
            if wallet::is_valid_address(&address) {
                println!("✅ '{}' is a valid Dogecoin address! Much valid. Wow.", address);
            } else {
                println!("❌ '{}' is NOT a valid Dogecoin address.", address);
                println!("   Valid addresses start with 'D' and are 34 characters long.");
            }
        }
    }
    Ok(())
}

/// Send a Dogecoin transaction over LoRa.
async fn cmd_send(port: &str, to: &str, amount: f64, memo: Option<&str>) -> Result<()> {
    if !wallet::is_valid_address(to) {
        anyhow::bail!("'{}' is not a valid Dogecoin address (must start with 'D')", to);
    }
    if amount <= 0.0 {
        anyhow::bail!("Amount must be > 0 DOGE");
    }

    println!("🐕 Connecting to {} ...", port);
    let manager = Arc::new(SerialManager::new());

    // Connect with a no-op packet handler (we just want to send)
    let on_packet = Arc::new(|_pkt: IncomingPacket| {});
    manager.connect(port, on_packet).await
        .with_context(|| format!("Failed to open serial port {}", port))?;

    println!("✅ Connected! Encoding transaction...");

    let payload = wallet::encode_transaction_payload(to, amount, memo)
        .context("Failed to encode transaction")?;

    let src = manager.get_node_address().await;
    let dst = NodeAddress::broadcast();

    // Send single or multipart depending on payload size
    if payload.len() <= radio::MAX_SINGLE_PAYLOAD_LEN {
        let pkt = radio::build_doge_tx(&src, &dst, &payload);
        manager.send_raw(pkt).await.context("Failed to send packet")?;
    } else {
        let pkts = radio::build_multipart_packets(&src, &dst, radio::CMD_DOGE_TX, &payload);
        let total = pkts.len();
        for (i, pkt) in pkts.into_iter().enumerate() {
            println!("  Sending part {}/{} ...", i + 1, total);
            manager.send_raw(pkt).await.context("Failed to send multipart packet")?;
            tokio::time::sleep(Duration::from_millis(50)).await;
        }
    }

    let memo_note = memo.map(|m| format!(" [{}]", m)).unwrap_or_default();
    println!("✅ Sent! {:.8} DOGE → {}{} via LoRa 🐕🌙", amount, to, memo_note);

    manager.disconnect().await.ok();
    Ok(())
}

/// Listen for incoming LoRa packets.
async fn cmd_receive(port: &str, timeout_secs: u64) -> Result<()> {
    println!("🐕 Connecting to {} and listening for packets...", port);
    if timeout_secs > 0 {
        println!("  (will stop after {} seconds; Ctrl-C to exit early)", timeout_secs);
    } else {
        println!("  (running forever; Ctrl-C to exit)");
    }

    let manager = Arc::new(SerialManager::new());

    let on_packet = Arc::new(|pkt: IncomingPacket| {
        println!(
            "[{}] 📻 {} → {} | cmd=0x{:02X} | rssi={} | {}",
            pkt.timestamp,
            pkt.source.to_display_string(),
            pkt.destination.to_display_string(),
            pkt.command,
            pkt.rssi,
            pkt.decoded.unwrap_or_else(|| format!("raw={}", pkt.payload_hex)),
        );
    });

    manager.connect(port, on_packet).await
        .with_context(|| format!("Failed to open serial port {}", port))?;

    println!("✅ Listening...\n");

    if timeout_secs > 0 {
        tokio::time::sleep(Duration::from_secs(timeout_secs)).await;
    } else {
        // Block until Ctrl-C
        tokio::signal::ctrl_c().await.context("Failed to listen for Ctrl-C")?;
    }

    println!("\n🐕 Done receiving. Disconnecting...");
    manager.disconnect().await.ok();
    Ok(())
}

/// Ping the Heltec device.
async fn cmd_ping(port: &str) -> Result<()> {
    println!("🐕 Connecting to {} ...", port);
    let manager = Arc::new(SerialManager::new());
    let on_packet = Arc::new(|_: IncomingPacket| {});
    manager.connect(port, on_packet).await
        .with_context(|| format!("Failed to open serial port {}", port))?;

    println!("Sending PING...");
    if manager.ping().await {
        println!("✅ PONG received! Device is alive. Much responsive. Wow. 🐕");
    } else {
        println!("❌ No response within 500 ms. Device may be offline or not running RadioDoge firmware.");
    }

    manager.disconnect().await.ok();
    Ok(())
}

/// Interactive REPL — replaces RadioDogeSharp's console menu.
async fn cmd_connect(port: &str) -> Result<()> {
    println!("🐕 RadioDoge Interactive Mode");
    println!("Connecting to {} ...", port);

    let manager = Arc::new(SerialManager::new());

    // Print packets as they arrive
    let on_packet = Arc::new(|pkt: IncomingPacket| {
        println!(
            "\n📻 PACKET  {} → {}  cmd=0x{:02X}  rssi={}",
            pkt.source.to_display_string(),
            pkt.destination.to_display_string(),
            pkt.command,
            pkt.rssi,
        );
        if let Some(decoded) = pkt.decoded {
            println!("   {}", decoded);
        }
        print!("\n> ");
    });

    manager.connect(port, on_packet).await
        .with_context(|| format!("Failed to open serial port {}", port))?;

    let addr = manager.get_node_address().await;
    println!("✅ Connected! Node address: {}\n", addr.to_display_string());
    println!("Commands: ping | wallet | send <addr> <amount> [memo] | stats | quit");
    println!("{}", "─".repeat(60));

    // Simple line-based REPL
    loop {
        print!("> ");
        let mut line = String::new();
        if std::io::stdin().read_line(&mut line).is_err() || line.trim().is_empty() {
            continue;
        }
        let parts: Vec<&str> = line.trim().split_whitespace().collect();
        match parts.as_slice() {
            ["quit"] | ["exit"] | ["q"] => {
                println!("👋 Disconnecting. Much goodbye. Wow.");
                break;
            }
            ["ping"] => {
                let ok = manager.ping().await;
                println!("{}", if ok { "✅ PONG!" } else { "❌ No response" });
            }
            ["wallet"] => {
                match wallet::generate_keypair() {
                    Ok(w) => {
                        println!("🆕 New wallet generated:");
                        println!("   Address:     {}", w.address);
                        println!("   Private key: {}", w.private_key_wif);
                        println!("   ⚠️  Save the private key now — not stored anywhere!");
                    }
                    Err(e) => println!("❌ Error: {}", e),
                }
            }
            ["send", to_addr, amount_str, memo @ ..] => {
                match amount_str.parse::<f64>() {
                    Err(_) => println!("❌ Invalid amount: {}", amount_str),
                    Ok(amount) => {
                        if !wallet::is_valid_address(to_addr) {
                            println!("❌ Invalid Dogecoin address: {}", to_addr);
                        } else {
                            let memo_str = if memo.is_empty() {
                                None
                            } else {
                                Some(memo.join(" "))
                            };
                            let payload = wallet::encode_transaction_payload(
                                to_addr,
                                amount,
                                memo_str.as_deref(),
                            );
                            match payload {
                                Err(e) => println!("❌ Encode error: {}", e),
                                Ok(bytes) => {
                                    let src = manager.get_node_address().await;
                                    let dst = NodeAddress::broadcast();
                                    let pkt = if bytes.len() <= radio::MAX_SINGLE_PAYLOAD_LEN {
                                        vec![radio::build_doge_tx(&src, &dst, &bytes)]
                                    } else {
                                        radio::build_multipart_packets(
                                            &src, &dst, radio::CMD_DOGE_TX, &bytes,
                                        )
                                    };
                                    let mut ok = true;
                                    for p in pkt {
                                        if manager.send_raw(p).await.is_err() {
                                            ok = false;
                                            break;
                                        }
                                    }
                                    if ok {
                                        println!("✅ Sent {:.8} DOGE → {} 🐕🌙", amount, to_addr);
                                    } else {
                                        println!("❌ Failed to send packet");
                                    }
                                }
                            }
                        }
                    }
                }
            }
            ["stats"] => {
                let s = manager.get_stats().await;
                println!("📊 Sent: {}  Received: {}  RSSI: {} dBm  SNR: {:.1} dB",
                    s.packets_sent, s.packets_received, s.rssi, s.snr);
            }
            ["help"] | [] => {
                println!("Commands:");
                println!("  ping                         — ping the device");
                println!("  wallet                       — generate new Dogecoin keypair");
                println!("  send <addr> <amount> [memo]  — send DOGE over LoRa");
                println!("  stats                        — show radio statistics");
                println!("  quit / exit / q              — disconnect and exit");
            }
            _ => {
                println!("❓ Unknown command. Type 'help' for a list.");
            }
        }
    }

    manager.disconnect().await.ok();
    Ok(())
}

/// Headless daemon — replaces serdog (C serial daemon).
///
/// Connects to the device, logs all received packets indefinitely.
/// Designed for Raspberry Pi gateway deployments.
async fn cmd_daemon(port: &str) -> Result<()> {
    println!("🐕 RadioDoge Daemon — Headless Mode (replaces serdog)");
    println!("Serial port: {}", port);
    println!("Press Ctrl-C to stop.\n");

    env_logger::Builder::from_env(
        env_logger::Env::default().default_filter_or("info"),
    )
    .format_timestamp_secs()
    .init();

    let manager = Arc::new(SerialManager::new());

    let on_packet = Arc::new(|pkt: IncomingPacket| {
        log::info!(
            "PACKET  {} → {}  cmd=0x{:02X}  rssi={}  payload={}{}",
            pkt.source.to_display_string(),
            pkt.destination.to_display_string(),
            pkt.command,
            pkt.rssi,
            pkt.payload_hex,
            pkt.decoded
                .as_deref()
                .map(|d| format!("  decoded={}", d))
                .unwrap_or_default(),
        );
    });

    log::info!("Connecting to {} ...", port);
    manager.connect(port, on_packet).await
        .with_context(|| format!("Failed to open serial port {}", port))?;

    let addr = manager.get_node_address().await;
    log::info!("Connected — node address: {}", addr.to_display_string());

    // Run until Ctrl-C
    tokio::signal::ctrl_c().await.context("Failed to listen for Ctrl-C")?;

    log::info!("Shutdown signal received — disconnecting");
    manager.disconnect().await.ok();

    Ok(())
}
