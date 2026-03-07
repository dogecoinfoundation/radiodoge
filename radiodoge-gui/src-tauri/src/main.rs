// Prevents an additional console window from appearing in release mode on Windows.
// This must be the first line in main.rs.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

fn main() {
    radiodoge_gui_lib::run();
}
