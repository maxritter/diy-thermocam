//! Confine paths built from outside data (camera SD paths, `FsEntry` names, recording frame
//! names, file arguments from the frontend) to the folder they belong in.

use std::fs;
use std::path::{Component, Path, PathBuf};

use thiserror::Error;

#[derive(Debug, Clone, PartialEq, Eq, Error)]
pub enum PathError {
    #[error("the path is empty")]
    Empty,
    #[error("the path contains a NUL byte")]
    Nul,
    #[error("the path is not a plain relative path: {0}")]
    NotRelative(String),
    #[error("the path climbs out of its folder: {0}")]
    Parent(String),
    #[error("the path leads outside its folder: {0}")]
    Escapes(String),
}

impl PathError {
    /// Error kind sent to the frontend.
    #[must_use]
    pub fn kind(&self) -> &'static str {
        "bad_path"
    }
}

/// Join an untrusted relative path to `root` and return it only if it stays inside `root`.
///
/// One leading `/` is stripped (camera paths are absolute on the card). `..`, absolute paths,
/// drive prefixes, UNC paths, backslashes, colons and NUL bytes are refused. The existing part of
/// the joined path is canonicalised (symlinks resolved) and must still start with the canonical
/// root. `root` itself need not exist yet.
pub fn confine(root: &Path, untrusted: &str) -> Result<PathBuf, PathError> {
    if untrusted.contains('\0') {
        return Err(PathError::Nul);
    }
    if untrusted.contains('\\') || untrusted.contains(':') {
        return Err(PathError::NotRelative(untrusted.to_string()));
    }
    let rel = untrusted.strip_prefix('/').unwrap_or(untrusted);
    if rel.starts_with('/') {
        return Err(PathError::NotRelative(untrusted.to_string()));
    }
    let mut parts = Vec::new();
    for part in rel.split('/') {
        match part {
            "" | "." => {}
            ".." => return Err(PathError::Parent(untrusted.to_string())),
            name => parts.push(name),
        }
    }
    if parts.is_empty() {
        return Err(PathError::Empty);
    }
    let mut joined = root.to_path_buf();
    for p in parts {
        joined.push(p);
    }
    check_inside(root, &joined, untrusted)
}

/// Normalise a path on the camera's card to `/a/b` form, refusing `..`, backslashes, colons
/// and NUL bytes (the firmware refuses them too; this keeps them out of cache and edit paths).
pub fn card_path(untrusted: &str) -> Result<String, PathError> {
    if untrusted.contains('\0') {
        return Err(PathError::Nul);
    }
    if untrusted.contains('\\') || untrusted.contains(':') {
        return Err(PathError::NotRelative(untrusted.to_string()));
    }
    let mut out = String::new();
    for part in untrusted.split('/') {
        match part {
            "" | "." => {}
            ".." => return Err(PathError::Parent(untrusted.to_string())),
            name => {
                out.push('/');
                out.push_str(name);
            }
        }
    }
    if out.is_empty() {
        return Err(PathError::Empty);
    }
    Ok(out)
}

/// Accept an absolute path only if it lies inside `root` (after resolving symlinks).
/// The root itself is refused.
pub fn ensure_within(root: &Path, candidate: &Path) -> Result<PathBuf, PathError> {
    let shown = candidate.to_string_lossy().into_owned();
    if shown.contains('\0') {
        return Err(PathError::Nul);
    }
    if !candidate.is_absolute() {
        return Err(PathError::NotRelative(shown));
    }
    if candidate
        .components()
        .any(|c| matches!(c, Component::ParentDir))
    {
        return Err(PathError::Parent(shown));
    }
    check_inside(root, candidate, &shown)
}

fn check_inside(root: &Path, joined: &Path, shown: &str) -> Result<PathBuf, PathError> {
    let root_resolved =
        resolve_existing_prefix(root).map_err(|()| PathError::Escapes(shown.into()))?;
    let resolved =
        resolve_existing_prefix(joined).map_err(|()| PathError::Escapes(shown.into()))?;
    if resolved == root_resolved || !resolved.starts_with(&root_resolved) {
        return Err(PathError::Escapes(shown.to_string()));
    }
    Ok(resolved)
}

/// Canonicalise the longest existing ancestor of `path` and append the rest unchanged.
/// A dangling symlink anywhere along the way is refused, since writing through it would
/// land wherever it points.
fn resolve_existing_prefix(path: &Path) -> Result<PathBuf, ()> {
    let mut rest: Vec<std::ffi::OsString> = Vec::new();
    let mut cur = path.to_path_buf();
    loop {
        if fs::symlink_metadata(&cur).is_ok() {
            let mut base = fs::canonicalize(&cur).map_err(|_| ())?;
            for part in rest.iter().rev() {
                base.push(part);
            }
            return Ok(base);
        }
        match (cur.file_name(), cur.parent()) {
            (Some(name), Some(parent)) => {
                rest.push(name.to_os_string());
                cur = parent.to_path_buf();
            }
            _ => {
                // Nothing along the way exists (relative or unrooted path): keep it as given.
                let mut base = cur;
                for part in rest.iter().rev() {
                    base.push(part);
                }
                return Ok(base);
            }
        }
    }
}
