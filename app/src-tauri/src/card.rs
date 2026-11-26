//! The connected camera's card, read one bounded round trip per lock (see `thermocam_core::card`).

use std::sync::Mutex;

use thermocam_core::card::{CardAccess, CardError};
use thermocam_core::protocol::messages::{FsListResp, FsStatResp};

use crate::error::CmdError;
use crate::state::ActiveTransport;

/// The shared client slot seen as a card; `NotConnected` when no camera is open.
pub struct LiveCard<'a>(pub &'a Mutex<Option<ActiveTransport>>);

impl LiveCard<'_> {
    fn with<R>(
        &self,
        f: impl FnOnce(&mut ActiveTransport) -> Result<R, thermocam_core::protocol::ProtocolError>,
    ) -> Result<R, CardError> {
        let mut guard = self.0.lock().map_err(|_| CardError::Poisoned)?;
        let client = guard.as_mut().ok_or(CardError::NotConnected)?;
        Ok(f(client)?)
    }
}

impl CardAccess for LiveCard<'_> {
    fn read_chunk(&self, path: &str, offset: u32, len: u32) -> Result<(Vec<u8>, bool), CardError> {
        self.with(|c| c.read_file_chunk(path, offset, len))
    }
    fn list_page(&self, path: &str, cursor: u32, max: u16) -> Result<FsListResp, CardError> {
        self.with(|c| c.list_dir_page(path, cursor, max))
    }
    fn stat(&self, path: &str) -> Result<FsStatResp, CardError> {
        self.with(|c| c.fs_stat(path))
    }
}

/// The connected camera's serial (`teensy_uid` as 16 lowercase hex digits), or `null`.
/// Camera captures name their camera with it (`{kind:"camera", serial, sd_path}`); the
/// number in `HelloResp.teensy_uid` can exceed what a JavaScript number holds exactly.
#[tauri::command]
pub fn cmd_camera_serial(
    state: tauri::State<'_, crate::state::AppState>,
) -> Result<Option<String>, CmdError> {
    Ok(state.camera_serial.lock()?.clone())
}

impl From<CardError> for CmdError {
    fn from(e: CardError) -> Self {
        match e {
            CardError::NotConnected => CmdError::not_connected(),
            other => CmdError::new("card", other.to_string()),
        }
    }
}
