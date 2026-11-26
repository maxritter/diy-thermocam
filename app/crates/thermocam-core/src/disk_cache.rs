//! A folder of cached files keyed by content identity (thumbnails, copied card videos),
//! with oldest-first eviction to keep it under a size cap.

use std::fs;
use std::io;
use std::path::{Path, PathBuf};
use std::time::SystemTime;

use sha2::{Digest, Sha256};

use crate::atomic_file::write_atomic;

#[derive(Debug, Clone)]
pub struct DiskCache {
    root: PathBuf,
}

impl DiskCache {
    #[must_use]
    pub fn new(root: PathBuf) -> Self {
        Self { root }
    }

    #[must_use]
    pub fn root(&self) -> &Path {
        &self.root
    }

    /// Stable file-name key for an identity such as `[path, mtime, size, palette]`.
    #[must_use]
    pub fn key(parts: &[&str]) -> String {
        let mut h = Sha256::new();
        for p in parts {
            h.update(p.as_bytes());
            h.update([0u8]);
        }
        let digest = h.finalize();
        digest.iter().take(16).map(|b| format!("{b:02x}")).collect()
    }

    fn path_for(&self, key: &str) -> PathBuf {
        self.root.join(format!("{key}.bin"))
    }

    #[must_use]
    pub fn get(&self, key: &str) -> Option<Vec<u8>> {
        let path = self.path_for(key);
        let bytes = fs::read(&path).ok()?;
        // Touch so eviction keeps recently used entries.
        if let Ok(f) = fs::File::options().write(true).open(&path) {
            let _ = f.set_modified(SystemTime::now());
        }
        Some(bytes)
    }

    pub fn put(&self, key: &str, bytes: &[u8]) -> io::Result<PathBuf> {
        let path = self.path_for(key);
        write_atomic(&path, bytes)?;
        Ok(path)
    }

    /// The cached bytes and `true`, or the result of `make` (stored) and `false`.
    pub fn get_or_insert_with<E, F>(&self, key: &str, make: F) -> Result<(Vec<u8>, bool), E>
    where
        F: FnOnce() -> Result<Vec<u8>, E>,
    {
        if let Some(bytes) = self.get(key) {
            return Ok((bytes, true));
        }
        let bytes = make()?;
        let _ = self.put(key, &bytes);
        Ok((bytes, false))
    }
}

fn entry_size(path: &Path) -> u64 {
    match fs::symlink_metadata(path) {
        Ok(m) if m.is_dir() => fs::read_dir(path)
            .map(|rd| rd.flatten().map(|e| entry_size(&e.path())).sum())
            .unwrap_or(0),
        Ok(m) => m.len(),
        Err(_) => 0,
    }
}

fn entry_mtime(path: &Path) -> SystemTime {
    let own = fs::metadata(path)
        .and_then(|m| m.modified())
        .unwrap_or(SystemTime::UNIX_EPOCH);
    if path.is_dir() {
        // A folder was last used when its newest file was written or touched.
        fs::read_dir(path)
            .ok()
            .and_then(|rd| {
                rd.flatten()
                    .filter_map(|e| e.metadata().and_then(|m| m.modified()).ok())
                    .max()
            })
            .unwrap_or(own)
    } else {
        own
    }
}

/// Remove the oldest direct children of `root` (files or folders) until the total size is at
/// most `cap_bytes`. `keep` is never removed. Returns the number of bytes freed.
pub fn evict_oldest(root: &Path, cap_bytes: u64, keep: Option<&Path>) -> io::Result<u64> {
    let mut entries: Vec<(SystemTime, u64, PathBuf)> = match fs::read_dir(root) {
        Ok(rd) => rd
            .flatten()
            .map(|e| {
                let p = e.path();
                (entry_mtime(&p), entry_size(&p), p)
            })
            .collect(),
        Err(e) if e.kind() == io::ErrorKind::NotFound => return Ok(0),
        Err(e) => return Err(e),
    };
    let mut total: u64 = entries.iter().map(|(_, s, _)| *s).sum();
    entries.sort_by_key(|(t, _, _)| *t);
    let mut freed = 0;
    for (_, size, path) in entries {
        if total <= cap_bytes {
            break;
        }
        if keep.is_some_and(|k| k == path) {
            continue;
        }
        let removed = if path.is_dir() {
            fs::remove_dir_all(&path)
        } else {
            fs::remove_file(&path)
        };
        if removed.is_ok() {
            total = total.saturating_sub(size);
            freed += size;
        }
    }
    Ok(freed)
}
