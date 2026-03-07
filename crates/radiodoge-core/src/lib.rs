//! # radiodoge-core
//!
//! Pure Rust library providing the RadioDoge protocol stack:
//!
//! - [`types`] — shared data types (NodeAddress, WalletInfo, packets, stats)
//! - [`wallet`] — Dogecoin keypair generation and transaction encoding (pure Rust, no FFI)
//! - [`radio`] — LoRa packet protocol encoding/decoding
//! - [`serial`] — cross-platform serial port management with async Tokio runtime
//!
//! This crate has **no Tauri dependency** — it is shared between:
//! - `radiodoge-gui` (Tauri 2 desktop GUI)
//! - `radiodoge-cli` (headless CLI replacing RadioDogeSharp and serdog)
//! - Future Android / embedded targets

pub mod radio;
pub mod serial;
pub mod types;
pub mod wallet;
