//! Tauri v2 desktop shell for DIY-Thermocam Studio.

pub mod commands;

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    commands::build_app()
        .run(tauri::generate_context!())
        .expect("error while running DIY-Thermocam Studio");
}
