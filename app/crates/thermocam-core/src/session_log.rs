//! The chart's session log: every region sample Live plots is appended to a CSV on disk, so
//! Save CSV… covers the whole session while the frontend keeps only a decimated copy.

use std::fs::{self, File, OpenOptions};
use std::io::{self, BufWriter, Write};
use std::path::{Path, PathBuf};
use std::time::{Duration, SystemTime};

use serde::{Deserialize, Serialize};

pub const LOG_HEADER: &str = "t_ms,region_id,value_c";
/// Logs older than this are deleted when a new session starts.
pub const MAX_LOG_AGE: Duration = Duration::from_secs(7 * 24 * 3600);

/// One plotted value: milliseconds since the session started, the region and its °C value.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct LogSample {
    pub t_ms: u64,
    pub region_id: String,
    pub value_c: f32,
}

pub struct SessionLog {
    path: PathBuf,
    out: BufWriter<File>,
    samples: u64,
}

fn csv_field(s: &str) -> String {
    if s.contains([',', '"', '\n', '\r']) {
        format!("\"{}\"", s.replace('"', "\"\""))
    } else {
        s.to_string()
    }
}

/// Delete `*.csv` logs in `dir` last written before `now - max_age`. Returns how many.
pub fn prune_old_logs(dir: &Path, now: SystemTime, max_age: Duration) -> io::Result<usize> {
    let cutoff = now.checked_sub(max_age).unwrap_or(SystemTime::UNIX_EPOCH);
    let mut removed = 0;
    let entries = match fs::read_dir(dir) {
        Ok(rd) => rd,
        Err(e) if e.kind() == io::ErrorKind::NotFound => return Ok(0),
        Err(e) => return Err(e),
    };
    for entry in entries.flatten() {
        let path = entry.path();
        let is_log = path
            .extension()
            .is_some_and(|e| e.eq_ignore_ascii_case("csv"));
        let modified = entry.metadata().and_then(|m| m.modified()).ok();
        if is_log && modified.is_some_and(|m| m < cutoff) && fs::remove_file(&path).is_ok() {
            removed += 1;
        }
    }
    Ok(removed)
}

impl SessionLog {
    /// Start a new log `<start epoch ms>.csv` in `dir`, deleting logs older than 7 days.
    pub fn start(dir: &Path, now: SystemTime) -> io::Result<Self> {
        fs::create_dir_all(dir)?;
        let _ = prune_old_logs(dir, now, MAX_LOG_AGE);
        let start_ms = now
            .duration_since(SystemTime::UNIX_EPOCH)
            .map_or(0, |d| d.as_millis());
        for n in 0..1000u32 {
            let name = if n == 0 {
                format!("{start_ms}.csv")
            } else {
                format!("{start_ms}-{n}.csv")
            };
            let path = dir.join(name);
            match OpenOptions::new().write(true).create_new(true).open(&path) {
                Ok(file) => {
                    let mut out = BufWriter::new(file);
                    writeln!(out, "{LOG_HEADER}")?;
                    out.flush()?;
                    return Ok(Self {
                        path,
                        out,
                        samples: 0,
                    });
                }
                Err(e) if e.kind() == io::ErrorKind::AlreadyExists => {}
                Err(e) => return Err(e),
            }
        }
        Err(io::Error::new(
            io::ErrorKind::AlreadyExists,
            "no free log name",
        ))
    }

    #[must_use]
    pub fn path(&self) -> &Path {
        &self.path
    }

    #[must_use]
    pub fn samples(&self) -> u64 {
        self.samples
    }

    /// Append a batch (about one per second) and flush it to disk.
    pub fn append(&mut self, batch: &[LogSample]) -> io::Result<()> {
        for s in batch {
            writeln!(
                self.out,
                "{},{},{:.2}",
                s.t_ms,
                csv_field(&s.region_id),
                s.value_c
            )?;
        }
        self.out.flush()?;
        self.samples += batch.len() as u64;
        Ok(())
    }

    /// Copy the whole session so far to `out_path`. Returns the bytes copied.
    pub fn export(&mut self, out_path: &Path) -> io::Result<u64> {
        self.out.flush()?;
        if let Some(parent) = out_path.parent().filter(|p| !p.as_os_str().is_empty()) {
            fs::create_dir_all(parent)?;
        }
        fs::copy(&self.path, out_path)
    }
}
