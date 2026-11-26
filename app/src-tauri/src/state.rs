//! Managed Studio state shared by every command: the camera client, the stream thread,
//! the latest frame, the active recording and the small in-memory caches.

use std::collections::{HashMap, VecDeque};
use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

use thermocam_core::palettes::{find_palette, Palette};
use thermocam_core::protocol::codec::{FrameRaw, HelloResp, StreamCtrl};
use thermocam_core::protocol::messages::{FsEntry, FsListResp, FsStatResp};
use thermocam_core::protocol::transport::{ProtocolClient, SerialTransport, SimulatorTransport};
use thermocam_core::protocol::{MsgType, ProtocolError};

use crate::playback::{RecordingCache, SeriesCache};
use crate::recorder::ActiveRecording;

const MAX_SD_CACHE_ENTRIES: usize = 64;

pub enum ActiveTransport {
    Serial(Box<ProtocolClient<SerialTransport>>),
    Simulator(Box<ProtocolClient<SimulatorTransport>>),
}

macro_rules! with_client {
    ($self:ident, $c:ident => $body:expr) => {
        match $self {
            ActiveTransport::Serial($c) => $body,
            ActiveTransport::Simulator($c) => $body,
        }
    };
}

impl ActiveTransport {
    pub fn hello(&mut self) -> Result<HelloResp, ProtocolError> {
        with_client!(self, c => c.hello())
    }
    pub fn set_stream(&mut self, ctrl: StreamCtrl) -> Result<StreamCtrl, String> {
        with_client!(self, c => c.set_stream(ctrl).map_err(|e| e.to_string()))
    }
    pub fn request_timeout(
        &mut self,
        mt: MsgType,
        payload: &[u8],
        timeout: Duration,
    ) -> Result<Vec<u8>, String> {
        let pkt =
            with_client!(self, c => c.request(mt, payload, timeout).map_err(|e| e.to_string())?);
        Ok(pkt.payload)
    }
    pub fn request(&mut self, mt: MsgType, payload: &[u8]) -> Result<Vec<u8>, String> {
        self.request_timeout(mt, payload, Duration::from_millis(2000))
    }
    pub fn send_ping_nowait(&mut self, host_time_us: u64) -> Result<(), String> {
        with_client!(self, c => c.send_ping_nowait(host_time_us).map_err(|e| e.to_string()))
    }
    pub fn poll_packets(
        &mut self,
        timeout: Duration,
    ) -> Result<Vec<thermocam_core::protocol::Packet>, String> {
        with_client!(self, c => c.poll_packets(timeout).map_err(|e| e.to_string()))
    }
    pub fn list_dir_page(
        &mut self,
        path: &str,
        cursor: u32,
        max_entries: u16,
    ) -> Result<FsListResp, ProtocolError> {
        with_client!(self, c => c.list_dir_page(path, cursor, max_entries))
    }
    pub fn read_file_chunk(
        &mut self,
        path: &str,
        offset: u32,
        len: u32,
    ) -> Result<(Vec<u8>, bool), ProtocolError> {
        with_client!(self, c => c.read_file_chunk(path, offset, len))
    }
    pub fn fs_stat(&mut self, path: &str) -> Result<FsStatResp, ProtocolError> {
        with_client!(self, c => c.fs_stat(path, false))
    }
    pub fn delete_file(&mut self, path: &str) -> Result<(), String> {
        with_client!(self, c => c.delete_file(path).map_err(|e| e.to_string()))
    }
    pub fn list_dir_all(&mut self, path: &str) -> Result<Vec<FsEntry>, String> {
        with_client!(self, c => c.list_dir_all(path).map_err(|e| e.to_string()))
    }
    pub fn read_file(&mut self, path: &str) -> Result<Vec<u8>, String> {
        with_client!(self, c => c.read_file_bytes(path).map_err(|e| e.to_string()))
    }
}

#[derive(Default)]
pub struct BoundedSdCache {
    map: HashMap<String, Vec<u8>>,
    order: VecDeque<String>,
}

impl BoundedSdCache {
    pub fn insert(&mut self, key: String, val: Vec<u8>) {
        if let Some(existing) = self.map.get_mut(&key) {
            *existing = val;
            return;
        }
        while self.map.len() >= MAX_SD_CACHE_ENTRIES {
            if let Some(oldest) = self.order.pop_front() {
                self.map.remove(&oldest);
            } else {
                break;
            }
        }
        self.order.push_back(key.clone());
        self.map.insert(key, val);
    }

    pub fn get(&self, key: &str) -> Option<&Vec<u8>> {
        self.map.get(key)
    }

    pub fn remove(&mut self, key: &str) {
        self.map.remove(key);
        self.order.retain(|k| k != key);
    }
}

pub struct AppState {
    pub client: Arc<Mutex<Option<ActiveTransport>>>,
    /// `teensy_uid` of the connected camera as 16 hex digits (`sim` cameras included).
    pub camera_serial: Arc<Mutex<Option<String>>>,
    pub streaming: Arc<AtomicBool>,
    pub stream_gen: Arc<AtomicU64>,
    pub stream_lifecycle: Arc<Mutex<()>>,
    pub stream_thread: Arc<Mutex<Option<thread::JoinHandle<()>>>>,
    pub latest_frame: Arc<Mutex<Option<Arc<FrameRaw>>>>,
    /// The recording or time-lapse being written by the stream thread.
    pub recording: Arc<Mutex<Option<ActiveRecording>>>,
    /// Folder of the most recently finished recording (the old export path uses it).
    pub last_recording: Arc<Mutex<Option<PathBuf>>>,
    pub custom_palettes: Arc<Mutex<HashMap<String, Palette>>>,
    pub sd_cache: Arc<Mutex<BoundedSdCache>>,
    pub recordings: Arc<Mutex<RecordingCache>>,
    pub series_cache: Arc<Mutex<SeriesCache>>,
    pub session_log: Arc<Mutex<Option<thermocam_core::session_log::SessionLog>>>,
}

impl Default for AppState {
    fn default() -> Self {
        Self {
            client: Arc::new(Mutex::new(None)),
            camera_serial: Arc::new(Mutex::new(None)),
            streaming: Arc::new(AtomicBool::new(false)),
            stream_gen: Arc::new(AtomicU64::new(0)),
            stream_lifecycle: Arc::new(Mutex::new(())),
            stream_thread: Arc::new(Mutex::new(None)),
            latest_frame: Arc::new(Mutex::new(None)),
            recording: Arc::new(Mutex::new(None)),
            last_recording: Arc::new(Mutex::new(None)),
            custom_palettes: Arc::new(Mutex::new(HashMap::new())),
            sd_cache: Arc::new(Mutex::new(BoundedSdCache::default())),
            recordings: Arc::new(Mutex::new(RecordingCache::default())),
            series_cache: Arc::new(Mutex::new(SeriesCache::default())),
            session_log: Arc::new(Mutex::new(None)),
        }
    }
}

pub fn stop_stream_and_join(
    streaming: &AtomicBool,
    stream_gen: &AtomicU64,
    stream_thread: &Mutex<Option<thread::JoinHandle<()>>>,
) {
    stream_gen.fetch_add(1, Ordering::SeqCst);
    streaming.store(false, Ordering::SeqCst);
    let handle = stream_thread.lock().ok().and_then(|mut g| g.take());
    if let Some(h) = handle {
        let _ = h.join();
    }
}

/// Hands back the client this Studio holds: a polite goodbye, then dropping it closes the
/// port. Connect calls this before opening a port, so a reconnect after the page reloads
/// never finds the port "in use" by Studio itself. Returns whether a client was held.
pub fn release_client(slot: &Mutex<Option<ActiveTransport>>) -> bool {
    let held = match slot.lock() {
        Ok(mut guard) => guard.take(),
        Err(poisoned) => poisoned.into_inner().take(),
    };
    match held {
        Some(mut client) => {
            let _ = client.request_timeout(MsgType::GoodbyeReq, &[], Duration::from_millis(500));
            drop(client);
            true
        }
        None => false,
    }
}

pub fn resolve_palette(state: &AppState, id: &str) -> Result<Palette, String> {
    if let Ok(p) = find_palette(id) {
        return Ok(p);
    }
    state
        .custom_palettes
        .lock()
        .map_err(|e| e.to_string())?
        .get(id)
        .cloned()
        .ok_or_else(|| format!("palette '{id}' not found"))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sim_client() -> ActiveTransport {
        ActiveTransport::Simulator(Box::new(ProtocolClient::new(SimulatorTransport::new())))
    }

    #[test]
    fn releasing_a_held_client_empties_the_slot_so_the_port_can_reopen() {
        let slot = Mutex::new(Some(sim_client()));
        assert!(release_client(&slot));
        assert!(slot.lock().unwrap().is_none());
    }

    #[test]
    fn releasing_an_empty_slot_does_nothing() {
        let slot: Mutex<Option<ActiveTransport>> = Mutex::new(None);
        assert!(!release_client(&slot));
        assert!(slot.lock().unwrap().is_none());
    }
}
