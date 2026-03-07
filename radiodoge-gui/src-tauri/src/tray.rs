//! System tray icon and menu for RadioDoge.
//!
//! The tray icon:
//!   - Shows a Dogecoin-branded icon in the system tray
//!   - Displays connection status in the menu
//!   - Left-click shows/hides the main window
//!   - Menu has "Open", "Status", and "Quit" items

use tauri::{
    menu::{Menu, MenuItem},
    tray::{MouseButton, TrayIconBuilder, TrayIconEvent},
    Manager, Runtime,
};

/// Build and register the system tray icon with its context menu.
pub fn setup_tray<R: Runtime>(app: &tauri::App<R>) -> tauri::Result<()> {
    let open_item = MenuItem::with_id(app, "open", "🐕 Open RadioDoge", true, None::<&str>)?;
    let status_item = MenuItem::with_id(app, "status", "⚪ Disconnected", false, None::<&str>)?;
    let separator = tauri::menu::PredefinedMenuItem::separator(app)?;
    let quit_item = MenuItem::with_id(app, "quit", "Quit", true, None::<&str>)?;

    let menu = Menu::with_items(app, &[&open_item, &status_item, &separator, &quit_item])?;

    TrayIconBuilder::new()
        .icon(app.default_window_icon().cloned().expect("app should have a window icon"))
        .menu(&menu)
        .tooltip("RadioDoge — Wireless Dogecoin")
        .on_tray_icon_event(|tray, event| {
            if let TrayIconEvent::Click { button: MouseButton::Left, .. } = event {
                // Left-click: show/focus main window
                let app = tray.app_handle();
                if let Some(window) = app.get_webview_window("main") {
                    if window.is_visible().unwrap_or(false) {
                        let _ = window.hide();
                    } else {
                        let _ = window.show();
                        let _ = window.set_focus();
                    }
                }
            }
        })
        .on_menu_event(|app, event| match event.id.as_ref() {
            "open" => {
                if let Some(window) = app.get_webview_window("main") {
                    let _ = window.show();
                    let _ = window.set_focus();
                }
            }
            "quit" => {
                app.exit(0);
            }
            _ => {}
        })
        .build(app)?;

    Ok(())
}

/// Update the tray status menu item text.
/// Call this whenever the connection state changes.
pub fn update_tray_status<R: Runtime>(app: &tauri::AppHandle<R>, connected: bool, port: Option<&str>) {
    // In Tauri 2, updating menu items requires re-building the menu or
    // using a stored handle. For MVP, we use the tooltip to show status.
    let tooltip = if connected {
        format!("RadioDoge — 🟢 Connected ({})", port.unwrap_or("?"))
    } else {
        "RadioDoge — 🔴 Disconnected".to_string()
    };

    // Update all tray icons (there should only be one)
    for tray in app.tray_by_id("") {
        let _ = tray.set_tooltip(Some(&tooltip));
    }
}
