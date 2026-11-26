//! The Captures folder: default location, writability check, collision-free names and the
//! snapshot `.DAT` writer.

use std::fs::{self, OpenOptions};
use std::io::{self, Write};
use std::path::{Path, PathBuf};

use serde::Serialize;

use crate::dat::ThermalDat;
use crate::protocol::codec::FrameRaw;

/// Firmware colour scheme written into snapshot `.DAT` files (Ironblack).
pub const SNAPSHOT_COLOR_SCHEME: u8 = 10;

/// A wall-clock time in the computer's time zone, used for capture names.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct LocalStamp {
    pub year: u16,
    pub month: u8,
    pub day: u8,
    pub hour: u8,
    pub minute: u8,
    pub second: u8,
}

impl LocalStamp {
    /// `YYYYMMDDHHMMSS`, the camera's own naming scheme.
    #[must_use]
    pub fn to_name(self) -> String {
        format!(
            "{:04}{:02}{:02}{:02}{:02}{:02}",
            self.year, self.month, self.day, self.hour, self.minute, self.second
        )
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum CaptureErrorKind {
    ReadOnly,
    NoSpace,
    NoFrame,
    Other,
}

impl CaptureErrorKind {
    #[must_use]
    pub fn as_str(self) -> &'static str {
        match self {
            Self::ReadOnly => "read_only",
            Self::NoSpace => "no_space",
            Self::NoFrame => "no_frame",
            Self::Other => "other",
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct CaptureError {
    pub kind: CaptureErrorKind,
    pub detail: String,
}

impl std::fmt::Display for CaptureError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}: {}", self.kind.as_str(), self.detail)
    }
}

impl std::error::Error for CaptureError {}

impl From<io::Error> for CaptureError {
    fn from(e: io::Error) -> Self {
        Self {
            kind: classify_io(&e),
            detail: e.to_string(),
        }
    }
}

/// ENOSPC on Linux/macOS, `ERROR_DISK_FULL` / `ERROR_HANDLE_DISK_FULL` on Windows.
const NO_SPACE_OS_CODES: &[i32] = &[28, 112, 39];
/// EROFS on Linux (30) and macOS (30), `ERROR_WRITE_PROTECT` on Windows (19).
const READ_ONLY_OS_CODES: &[i32] = &[30, 19];

/// Map an I/O error to the kinds the frontend explains.
#[must_use]
pub fn classify_io(e: &io::Error) -> CaptureErrorKind {
    match e.kind() {
        io::ErrorKind::PermissionDenied | io::ErrorKind::ReadOnlyFilesystem => {
            CaptureErrorKind::ReadOnly
        }
        io::ErrorKind::StorageFull | io::ErrorKind::QuotaExceeded => CaptureErrorKind::NoSpace,
        _ => match e.raw_os_error() {
            Some(code) if NO_SPACE_OS_CODES.contains(&code) => CaptureErrorKind::NoSpace,
            Some(code) if READ_ONLY_OS_CODES.contains(&code) => CaptureErrorKind::ReadOnly,
            _ => CaptureErrorKind::Other,
        },
    }
}

/// `picture_dir/Thermocam`, or `home/Pictures/Thermocam` when the system has no picture folder.
#[must_use]
pub fn default_captures_dir(
    picture_dir: Option<&Path>,
    home_dir: Option<&Path>,
) -> Option<PathBuf> {
    picture_dir
        .map(Path::to_path_buf)
        .or_else(|| home_dir.map(|h| h.join("Pictures")))
        .map(|p| p.join("Thermocam"))
}

/// Create `dir` if needed and prove a file can be created in it (the probe is removed again).
pub fn check_writable(dir: &Path) -> Result<(), CaptureError> {
    fs::create_dir_all(dir)?;
    let probe = dir.join(".thermocam-write-test");
    let mut f = OpenOptions::new()
        .write(true)
        .create(true)
        .truncate(true)
        .open(&probe)?;
    let wrote = f.write_all(b"ok");
    drop(f);
    let _ = fs::remove_file(&probe);
    wrote?;
    Ok(())
}

/// First free path among `stem.ext`, `stem-2.ext`, `stem-3.ext`, … (`ext` `None` for folders).
#[must_use]
pub fn unique_path(dir: &Path, stem: &str, ext: Option<&str>) -> PathBuf {
    let name = |n: u32| -> String {
        let base = if n <= 1 {
            stem.to_string()
        } else {
            format!("{stem}-{n}")
        };
        match ext {
            Some(e) => format!("{base}.{e}"),
            None => base,
        }
    };
    let mut n = 1;
    loop {
        let candidate = dir.join(name(n));
        if fs::symlink_metadata(&candidate).is_err() {
            return candidate;
        }
        n += 1;
    }
}

/// Write `bytes` to a new file named after `stem` in `dir`, never overwriting an existing file.
pub fn write_new_file(
    dir: &Path,
    stem: &str,
    ext: &str,
    bytes: &[u8],
) -> Result<PathBuf, CaptureError> {
    fs::create_dir_all(dir)?;
    for _ in 0..10_000 {
        let path = unique_path(dir, stem, Some(ext));
        match OpenOptions::new().write(true).create_new(true).open(&path) {
            Ok(mut f) => {
                if let Err(e) = f.write_all(bytes).and_then(|()| f.sync_all()) {
                    drop(f);
                    let _ = fs::remove_file(&path);
                    return Err(e.into());
                }
                return Ok(path);
            }
            Err(e) if e.kind() == io::ErrorKind::AlreadyExists => {}
            Err(e) => return Err(e.into()),
        }
    }
    Err(CaptureError {
        kind: CaptureErrorKind::Other,
        detail: "no free file name".into(),
    })
}

/// Check each path the frontend wants moved to the Trash: it must lie inside `captures_root`.
#[must_use]
pub fn trash_targets(
    captures_root: &Path,
    paths: &[String],
) -> Vec<Result<PathBuf, crate::paths::PathError>> {
    paths
        .iter()
        .map(|p| crate::paths::ensure_within(captures_root, Path::new(p)))
        .collect()
}

/// What an entry of the Captures folder is.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum LocalCaptureKind {
    /// A single `.DAT` file.
    Snapshot,
    /// A folder of `.DAT` frames (Studio's recordings and time-lapses, copied camera videos).
    Recording,
    /// Anything else, listed under "Other files".
    Other,
}

impl LocalCaptureKind {
    #[must_use]
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Snapshot => "snapshot",
            Self::Recording => "recording",
            Self::Other => "other",
        }
    }
}

/// One entry of the Captures folder for the library's "On this computer" tab.
#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct LocalCapture {
    pub name: String,
    pub path: String,
    pub kind: LocalCaptureKind,
    /// Bytes of the file, or of a recording's frames.
    pub size: u64,
    /// Frames in a recording folder.
    pub frames: Option<u32>,
    pub mtime_ms: u64,
}

/// Compares bytes, so a name with non-ASCII characters never splits inside one.
fn has_ext(name: &str, ext: &str) -> bool {
    let (n, e) = (name.as_bytes(), ext.as_bytes());
    n.len() > e.len() && n[n.len() - e.len()..].eq_ignore_ascii_case(e)
}

fn mtime_ms(meta: &fs::Metadata) -> u64 {
    meta.modified()
        .ok()
        .and_then(|t| t.duration_since(std::time::UNIX_EPOCH).ok())
        .map_or(0, |d| u64::try_from(d.as_millis()).unwrap_or(u64::MAX))
}

/// The entries of `dir` (one level). Edits (`.thermo.json`, `.thermo.json.bad`), a snapshot's
/// PNG beside its `.DAT`, and hidden files belong to their capture and are left out.
pub fn list_captures(dir: &Path) -> io::Result<Vec<LocalCapture>> {
    let mut names = std::collections::HashSet::new();
    let mut entries = Vec::new();
    for entry in fs::read_dir(dir)?.flatten() {
        let Some(name) = entry.file_name().to_str().map(str::to_string) else {
            continue;
        };
        names.insert(name.to_ascii_lowercase());
        entries.push((name, entry));
    }
    let mut out = Vec::new();
    for (name, entry) in entries {
        if name.starts_with('.')
            || has_ext(&name, ".thermo.json")
            || has_ext(&name, ".thermo.json.bad")
        {
            continue;
        }
        if has_ext(&name, ".png") {
            let stem = &name[..name.len() - 4];
            if names.contains(&format!("{stem}.dat").to_ascii_lowercase()) {
                continue;
            }
        }
        let Ok(meta) = entry.metadata() else { continue };
        let path = entry.path().to_string_lossy().into_owned();
        if meta.is_dir() {
            let mut frames = 0u32;
            let mut size = 0u64;
            // A folder Studio may not read (another app's library) is listed as another file,
            // never failing the whole list.
            let inner = fs::read_dir(entry.path()).into_iter().flatten().flatten();
            for f in inner {
                let is_frame = f.file_name().to_str().is_some_and(|n| has_ext(n, ".dat"));
                if let (true, Ok(m)) = (is_frame, f.metadata()) {
                    if m.is_file() {
                        frames += 1;
                        size += m.len();
                    }
                }
            }
            let kind = if frames > 0 {
                LocalCaptureKind::Recording
            } else {
                LocalCaptureKind::Other
            };
            out.push(LocalCapture {
                name,
                path,
                kind,
                size,
                frames: (frames > 0).then_some(frames),
                mtime_ms: mtime_ms(&meta),
            });
        } else if meta.is_file() {
            let kind = if has_ext(&name, ".dat") {
                LocalCaptureKind::Snapshot
            } else {
                LocalCaptureKind::Other
            };
            out.push(LocalCapture {
                name,
                path,
                kind,
                size: meta.len(),
                frames: None,
                mtime_ms: mtime_ms(&meta),
            });
        }
    }
    Ok(out)
}

/// Save `frame` as `YYYYMMDDHHMMSS.DAT` (V4 layout, 38,805 bytes for 160 × 120) in `dir`.
pub fn write_snapshot(
    dir: &Path,
    frame: &FrameRaw,
    now: LocalStamp,
) -> Result<PathBuf, CaptureError> {
    let dat = ThermalDat::from_frame(frame, SNAPSHOT_COLOR_SCHEME, 0);
    write_new_file(dir, &now.to_name(), "DAT", &dat.to_bytes())
}
