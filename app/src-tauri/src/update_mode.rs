//! "Finish update": a camera left waiting in update mode (Studio closed during an update).

use thermocam_core::flasher::{update_mode_present, RealHidBus};

/// True when the HalfKay bootloader is on USB. Lists HID devices only; never opens one.
#[tauri::command]
pub async fn cmd_update_mode_present() -> bool {
    tauri::async_runtime::spawn_blocking(|| update_mode_present(&RealHidBus))
        .await
        .unwrap_or(false)
}
