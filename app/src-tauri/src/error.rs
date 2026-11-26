//! The error payload every new command rejects with: `{ kind, detail }`.

use serde::Serialize;

use thermocam_core::captures::CaptureError;
use thermocam_core::paths::PathError;

#[derive(Debug, Clone, Serialize)]
pub struct CmdError {
    pub kind: String,
    pub detail: String,
}

impl CmdError {
    pub fn new(kind: &str, detail: impl Into<String>) -> Self {
        Self {
            kind: kind.to_string(),
            detail: detail.into(),
        }
    }

    pub fn other(detail: impl Into<String>) -> Self {
        Self::new("other", detail)
    }

    pub fn not_connected() -> Self {
        Self::new("not_connected", "No camera is connected.")
    }
}

impl From<CaptureError> for CmdError {
    fn from(e: CaptureError) -> Self {
        Self::new(e.kind.as_str(), e.detail)
    }
}

impl From<PathError> for CmdError {
    fn from(e: PathError) -> Self {
        Self::new(e.kind(), e.to_string())
    }
}

impl From<std::io::Error> for CmdError {
    fn from(e: std::io::Error) -> Self {
        CaptureError::from(e).into()
    }
}

impl From<tauri::Error> for CmdError {
    fn from(e: tauri::Error) -> Self {
        Self::other(e.to_string())
    }
}

impl<T> From<std::sync::PoisonError<T>> for CmdError {
    fn from(e: std::sync::PoisonError<T>) -> Self {
        Self::other(e.to_string())
    }
}

pub type CmdResult<T> = Result<T, CmdError>;
