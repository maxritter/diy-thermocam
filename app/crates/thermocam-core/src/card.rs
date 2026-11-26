//! Reading the camera's SD card while Live streams: every operation takes the client lock for
//! one bounded round trip (one `FS_READ` chunk or one `FS_LIST` page) and releases it, so the
//! stream loop drains the frames that queued up meanwhile at most one round trip later.

use std::sync::Mutex;

use thiserror::Error;

use crate::protocol::messages::{FsEntry, FsListResp, FsStatResp};
use crate::protocol::transport::{ProtocolClient, Transport};
use crate::protocol::ProtocolError;

pub use crate::protocol::transport::MAX_READ_CHUNK;

/// Largest card file Studio reads into memory (a `.DAT` is 38,805 bytes).
pub const MAX_CARD_FILE_BYTES: usize = 64 * 1024 * 1024;
const LIST_PAGE: u16 = 64;
const MAX_LIST_PAGES: usize = 1024;

#[derive(Debug, Error)]
pub enum CardError {
    #[error("no camera is connected")]
    NotConnected,
    #[error("{0}")]
    Protocol(#[from] ProtocolError),
    #[error("the camera connection is unavailable")]
    Poisoned,
    #[error("the card file is larger than {MAX_CARD_FILE_BYTES} bytes")]
    TooLarge,
}

/// One bounded card operation at a time; implementors lock only for its duration.
pub trait CardAccess {
    fn read_chunk(&self, path: &str, offset: u32, len: u32) -> Result<(Vec<u8>, bool), CardError>;
    fn list_page(&self, path: &str, cursor: u32, max: u16) -> Result<FsListResp, CardError>;
    fn stat(&self, path: &str) -> Result<FsStatResp, CardError>;
}

impl<T: Transport> CardAccess for Mutex<ProtocolClient<T>> {
    fn read_chunk(&self, path: &str, offset: u32, len: u32) -> Result<(Vec<u8>, bool), CardError> {
        let mut c = self.lock().map_err(|_| CardError::Poisoned)?;
        Ok(c.read_file_chunk(path, offset, len)?)
    }
    fn list_page(&self, path: &str, cursor: u32, max: u16) -> Result<FsListResp, CardError> {
        let mut c = self.lock().map_err(|_| CardError::Poisoned)?;
        Ok(c.list_dir_page(path, cursor, max)?)
    }
    fn stat(&self, path: &str) -> Result<FsStatResp, CardError> {
        let mut c = self.lock().map_err(|_| CardError::Poisoned)?;
        Ok(c.fs_stat(path, false)?)
    }
}

/// Read a whole card file, re-taking the lock for every 32 KB chunk.
pub fn read_card_file<C: CardAccess + ?Sized>(card: &C, path: &str) -> Result<Vec<u8>, CardError> {
    let mut out = Vec::new();
    loop {
        let (chunk, eof) = card.read_chunk(path, out.len() as u32, MAX_READ_CHUNK)?;
        let empty = chunk.is_empty();
        out.extend_from_slice(&chunk);
        if out.len() > MAX_CARD_FILE_BYTES {
            return Err(CardError::TooLarge);
        }
        if eof || empty {
            return Ok(out);
        }
    }
}

/// List a card folder page by page, re-taking the lock for every page.
pub fn list_card_dir<C: CardAccess + ?Sized>(
    card: &C,
    path: &str,
) -> Result<Vec<FsEntry>, CardError> {
    let mut cursor = 0u32;
    let mut all = Vec::new();
    for _ in 0..MAX_LIST_PAGES {
        let page = card.list_page(path, cursor, LIST_PAGE)?;
        let got_any = !page.entries.is_empty();
        all.extend(page.entries);
        if page.next_cursor == u32::MAX || page.next_cursor == cursor || !got_any {
            break;
        }
        cursor = page.next_cursor;
    }
    Ok(all)
}
