//! Tauri v2 desktop shell for DIY-Thermocam Studio.

pub mod captures;
pub mod card;
pub mod commands;
pub mod edits;
pub mod error;
pub mod export;
pub mod pictures;
pub mod platform;
pub mod playback;
pub mod recorder;
pub mod session;
pub mod settings;
pub mod state;
pub mod thumbs;

use state::AppState;

/// Build the configured Tauri v2 application.
pub fn build_app() -> tauri::Builder<tauri::Wry> {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_notification::init())
        .manage(AppState::default())
        .invoke_handler(tauri::generate_handler![
            commands::cmd_list_ports,
            commands::cmd_get_palettes,
            commands::cmd_get_material_presets,
            commands::cmd_import_custom_palette,
            commands::cmd_connect,
            commands::cmd_disconnect,
            commands::cmd_start_stream,
            commands::cmd_stop_stream,
            commands::cmd_get_config,
            commands::cmd_set_config,
            commands::cmd_trigger_ffc,
            commands::cmd_get_diagnostics,
            commands::cmd_get_perf,
            commands::cmd_get_rad_params,
            commands::cmd_set_rad_params,
            commands::cmd_sd_list,
            commands::cmd_sd_read_dat,
            commands::cmd_sd_delete,
            commands::cmd_sd_download,
            commands::cmd_load_dat_file,
            commands::cmd_save_sidecar,
            commands::cmd_verify_hex,
            commands::cmd_pick_hex_file,
            export::cmd_export_media,
            commands::cmd_pick_directory,
            commands::cmd_capture_timelapse_frame,
            commands::cmd_toggle_recording,
            commands::cmd_evaluate_rois,
            commands::cmd_flash_firmware,
            settings::cmd_settings_load,
            settings::cmd_settings_save,
            captures::cmd_captures_dir,
            captures::cmd_set_captures_dir,
            captures::cmd_snapshot,
            captures::cmd_trash,
            pictures::cmd_render_png,
            thumbs::cmd_thumbnail,
            recorder::cmd_record_start,
            recorder::cmd_record_stop,
            recorder::cmd_record_status,
            playback::cmd_recording_open,
            playback::cmd_recording_frame,
            playback::cmd_sd_fetch_folder,
            playback::cmd_recording_series,
            session::cmd_session_log_start,
            session::cmd_session_log_append,
            session::cmd_session_log_export,
            card::cmd_camera_serial,
            edits::cmd_sidecar_load,
            edits::cmd_sidecar_save,
            edits::cmd_sidecar_reset,
        ])
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    build_app()
        .run(tauri::generate_context!())
        .expect("error while running DIY-Thermocam Studio");
}
