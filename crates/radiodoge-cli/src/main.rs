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

    /// Query the confirmed Dogecoin balance for an address via Trezor Blockbook
    ///
    /// Requires an internet connection.
    ///
    /// Example: radiodoge-cli balance -a DH5yaieqoZN36fDVciNyRueRGvGLR3mr7L
    Balance {
        /// Dogecoin address to query
        #[arg(short, long)]
        address: String,
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

    /// Generate a new wallet with a 12-word BIP39 mnemonic recovery phrase
    ///
    /// Derives the key at m/44'/3'/0'/0/0 (Dogecoin BIP44 path).
    /// Write down the phrase offline — it is shown once and never stored.
    ///
    /// Example: radiodoge-cli wallet mnemonic
    Mnemonic,

    /// Import a wallet from a BIP39 mnemonic recovery phrase
    ///
    /// Derives the key at m/44'/3'/0'/0/0 (Dogecoin BIP44 coin type 3).
    ///
    /// Example: radiodoge-cli wallet import-mnemonic "word1 word2 ... word12"
    ImportMnemonic {
        /// 12 or 24-word BIP39 mnemonic phrase (space-separated, quoted)
        phrase: String,
    },

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
        Commands::Balance { address } => cmd_balance(&address).await,
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
        WalletCommands::Mnemonic => {
            let mnemonic = wallet::generate_mnemonic().context("Failed to generate mnemonic")?;
            let phrase = mnemonic.to_string();
            let w = wallet::wallet_from_mnemonic(&phrase).context("Failed to derive wallet")?;
            println!("🌱 New Dogecoin Wallet with Recovery Phrase — WRITE THIS DOWN OFFLINE!\n");
            println!("  Recovery Phrase (12 words, SECRET):");
            for (i, word) in phrase.split_whitespace().enumerate() {
                println!("    {:2}. {}", i + 1, word);
            }
            println!();
            println!("  Derived at: m/44'/3'/0'/0/0 (Dogecoin BIP44)");
            println!("  Address (share this):     {}", w.address);
            println!("  Public Key (hex):          {}", w.public_key_hex);
            println!("  Private Key (WIF, SECRET): {}", w.private_key_wif);
            println!("\n⚠️  Store the recovery phrase OFFLINE. Anyone with it controls your DOGE.");
        }
        WalletCommands::ImportMnemonic { phrase } => {
            let w = wallet::wallet_from_mnemonic(&phrase)
                .context("Failed to derive wallet from mnemonic")?;
            println!("✅ Wallet restored from recovery phrase (m/44'/3'/0'/0/0)\n");
            println!("  Address (share this):     {}", w.address);
            println!("  Public Key (hex):          {}", w.public_key_hex);
            println!("  Private Key (WIF, SECRET): {}", w.private_key_wif);
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
    println!("Commands: ping | wallet | wallet-mnemonic | balance <addr> | send <addr> <amount> [memo] | stats | quit");
    println!("{}", "─".repeat(60));

    // Simple line-based REPL
    loop {
        print!("> ");
        let mut line = String::new();
        if std::io::stdin().read_line(&mut line).is_err() || line.trim().is_empty() {
            continue;
        }
        let parts: Vec<&str> = line.split_whitespace().collect();
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
            ["wallet-mnemonic"] => {
                match wallet::generate_mnemonic().and_then(|m| {
                    let phrase = m.to_string();
                    wallet::wallet_from_mnemonic(&phrase).map(|w| (phrase, w))
                }) {
                    Ok((phrase, w)) => {
                        println!("🌱 New wallet with recovery phrase:");
                        for (i, word) in phrase.split_whitespace().enumerate() {
                            println!("   {:2}. {}", i + 1, word);
                        }
                        println!("   Address: {}", w.address);
                        println!("   ⚠️  Write the phrase offline — shows once!");
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
            ["balance", addr] => {
                if !wallet::is_valid_address(addr) {
                    println!("❌ '{}' is not a valid Dogecoin address", addr);
                } else {
                    print!("🌐 Querying Blockbook... ");
                    match wallet::fetch_balance_blockbook(addr).await {
                        Ok(k) => println!("💰 {:.8} DOGE", k as f64 / 1e8),
                        Err(e) => println!("❌ {}", e),
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
                println!("  wallet-mnemonic              — generate wallet with 12-word BIP39 phrase");
                println!("  balance <addr>               — query confirmed balance via Blockbook");
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
/// When a signed Dogecoin transaction is received (CMD_DOGE_TX with a raw
/// transaction payload), broadcasts it to the Dogecoin network via Trezor
/// Blockbook and sends a TX_ACK message back to the originator.
///
/// Designed for Raspberry Pi gateway deployments.
async fn cmd_daemon(port: &str) -> Result<()> {
    println!("🐕 RadioDoge Daemon — Gateway Mode (replaces serdog)");
    println!("Serial port: {}", port);
    println!("Press Ctrl-C to stop.\n");

    env_logger::Builder::from_env(
        env_logger::Env::default().default_filter_or("info"),
    )
    .format_timestamp_secs()
    .init();

    let manager = Arc::new(SerialManager::new());
    let manager_for_ack = Arc::clone(&manager);

    let on_packet = Arc::new(move |pkt: IncomingPacket| {
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

        // When a signed Dogecoin transaction arrives, broadcast it to the network.
        if pkt.command == radio::CMD_DOGE_TX {
            let payload_bytes = hex::decode(&pkt.payload_hex).unwrap_or_default();
            if wallet::is_signed_tx_payload(&payload_bytes) {
                let raw_hex = pkt.payload_hex.clone();
                let mgr = Arc::clone(&manager_for_ack);
                let source = pkt.source.clone();
                log::info!(
                    "GATEWAY  signed tx detected ({} bytes) from {} — broadcasting to Dogecoin network",
                    payload_bytes.len(),
                    source.to_display_string()
                );
                tokio::spawn(async move {
                    daemon_broadcast_and_ack(raw_hex, mgr, source).await;
                });
            }
        }

        // When a balance request arrives, query Blockbook and send the result back.
        if pkt.command == radio::CMD_REQUEST_BALANCE {
            let payload_bytes = hex::decode(&pkt.payload_hex).unwrap_or_default();
            let addr = String::from_utf8_lossy(&payload_bytes)
                .trim_matches('\0')
                .trim()
                .to_string();
            if !addr.is_empty() {
                let mgr = Arc::clone(&manager_for_ack);
                let source = pkt.source.clone();
                log::info!("GATEWAY  balance request from {} for {}", source.to_display_string(), addr);
                tokio::spawn(async move {
                    daemon_fetch_and_send_balance(addr, mgr, source).await;
                });
            }
        }
    });

    log::info!("Connecting to {} ...", port);
    manager.connect(port, on_packet).await
        .with_context(|| format!("Failed to open serial port {}", port))?;

    let addr = manager.get_node_address().await;
    log::info!("Connected — node address: {}", addr.to_display_string());
    log::info!("Gateway ready — monitoring for signed Dogecoin transactions");

    // Run until Ctrl-C
    tokio::signal::ctrl_c().await.context("Failed to listen for Ctrl-C")?;

    log::info!("Shutdown signal received — disconnecting");
    manager.disconnect().await.ok();

    Ok(())
}

/// Broadcast a signed transaction to the Dogecoin network with exponential-backoff
/// retry (up to 3 attempts), then radio an ACK back to the originating node.
async fn daemon_broadcast_and_ack(
    raw_hex: String,
    mgr: Arc<SerialManager>,
    source: NodeAddress,
) {
    let mut delay_secs = 2u64;
    for attempt in 0..3u32 {
        if attempt > 0 {
            tokio::time::sleep(Duration::from_secs(delay_secs)).await;
            delay_secs *= 2;
        }
        match wallet::broadcast_raw_tx(&raw_hex).await {
            Ok(txid) => {
                log::info!("GATEWAY  broadcast OK  txid={}", txid);
                // Send ACK back to the originating node via radio
                let gateway_addr = mgr.get_node_address().await;
                let ack_msg = format!("TX_ACK:{}", &txid[..txid.len().min(40)]);
                let ack_pkt = radio::build_message(&gateway_addr, &source, &ack_msg);
                if let Err(e) = mgr.send_raw(ack_pkt).await {
                    log::warn!("GATEWAY  ACK send failed: {}", e);
                }
                return;
            }
            Err(e) => {
                log::warn!(
                    "GATEWAY  broadcast attempt {}/3 failed: {}",
                    attempt + 1, e
                );
            }
        }
    }
    log::error!(
        "GATEWAY  broadcast failed after 3 attempts for tx {}…",
        &raw_hex[..raw_hex.len().min(16)]
    );
}

/// Fetch the balance for `address` from Blockbook and send it back to `source` via radio.
/// The response is a CMD_MESSAGE with text `"BAL:{koinus}"` so the GUI can parse it.
async fn daemon_fetch_and_send_balance(
    address: String,
    mgr: Arc<SerialManager>,
    source: NodeAddress,
) {
    match wallet::fetch_balance_blockbook(&address).await {
        Ok(koinus) => {
            log::info!(
                "GATEWAY  balance for {}: {} koinus ({:.8} DOGE)",
                address, koinus, koinus as f64 / 1e8
            );
            let gateway_addr = mgr.get_node_address().await;
            let msg = format!("BAL:{}", koinus);
            let pkt = radio::build_message(&gateway_addr, &source, &msg);
            if let Err(e) = mgr.send_raw(pkt).await {
                log::warn!("GATEWAY  balance reply send failed: {}", e);
            }
        }
        Err(e) => {
            log::warn!("GATEWAY  balance fetch failed for {}: {}", address, e);
        }
    }
}

/// Query the confirmed balance for a Dogecoin address from Trezor Blockbook.
async fn cmd_balance(address: &str) -> Result<()> {
    if !wallet::is_valid_address(address) {
        anyhow::bail!("'{}' is not a valid Dogecoin address (must start with 'D')", address);
    }
    println!("🌐 Querying Blockbook for {}...", address);
    let koinus = wallet::fetch_balance_blockbook(address).await?;
    println!("💰 Balance: {:.8} DOGE  ({} koinus)", koinus as f64 / 1e8, koinus);
    Ok(())
}
