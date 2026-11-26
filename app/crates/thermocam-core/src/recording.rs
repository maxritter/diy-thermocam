//! Recordings on disk: a folder of `NNNNNN.DAT` frames plus `recording.json` with their times.
//! The writer runs in the stream thread and keeps only 4 bytes of timing per frame in memory,
//! so a recording's length is limited by disk space, not RAM.

use std::fs;
use std::io;
use std::path::{Path, PathBuf};

use serde::{Deserialize, Serialize};

use crate::atomic_file::write_atomic;
use crate::captures::classify_io;
use crate::dat::ThermalDat;
use crate::protocol::codec::FrameRaw;
use crate::radiometry::Material;

pub const RECORDING_MANIFEST: &str = "recording.json";
pub const RECORDING_SCHEMA_VERSION: u32 = 1;
/// `recording.json` is rewritten at least this often while recording.
pub const MANIFEST_REWRITE_MS: u32 = 5_000;
/// Record refuses to start below this much free space.
pub const MIN_FREE_TO_START: u64 = 500 * 1024 * 1024;
/// A running recording stops cleanly below this much free space.
pub const MIN_FREE_TO_CONTINUE: u64 = 200 * 1024 * 1024;
/// Nominal Lepton frame rate, used when a recording has no usable times.
pub const NOMINAL_FPS: f32 = 8.78;
const SPACE_CHECK_FRAMES: u32 = 10;
const SPACE_CHECK_MS: u32 = 1_000;
const DAT_COLOR_SCHEME: u8 = 10;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum RecordingSource {
    /// Recorded by Studio from the live stream.
    Studio,
    /// Studio's time-lapse (one frame per interval).
    Timelapse,
    /// A video folder from the camera's card (no `recording.json`, no times).
    Camera,
}

/// `recording.json`.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct RecordingManifest {
    pub schema_version: u32,
    pub source: RecordingSource,
    pub frames: usize,
    pub fps_nominal: f32,
    /// Milliseconds since the recording started, one per frame.
    pub t_ms: Vec<u32>,
    /// The sensor's material while recording (`None` = factory).
    pub sensor_material: Option<Material>,
}

/// Read `dir/recording.json`; `Ok(None)` when the folder has none (a camera video).
pub fn read_manifest(dir: &Path) -> io::Result<Option<RecordingManifest>> {
    match fs::read(dir.join(RECORDING_MANIFEST)) {
        Ok(bytes) => serde_json::from_slice(&bytes)
            .map(Some)
            .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e)),
        Err(e) if e.kind() == io::ErrorKind::NotFound => Ok(None),
        Err(e) => Err(e),
    }
}

/// Name of frame `index` inside a recording folder.
#[must_use]
pub fn frame_file_name(index: usize) -> String {
    format!("{index:06}.DAT")
}

/// An opened recording: what the player needs, with frames read from disk on demand.
#[derive(Debug, Clone, Serialize)]
pub struct RecordingIndex {
    pub frames: usize,
    /// Milliseconds since start, one per frame; `None` for camera videos (no times).
    pub t_ms: Option<Vec<u32>>,
    pub width: u16,
    pub height: u16,
    pub source: RecordingSource,
    pub fps_nominal: f32,
    pub sensor_material: Option<Material>,
    #[serde(skip)]
    pub files: Vec<PathBuf>,
}

/// True for a file name ending in `.DAT` (any case).
pub fn is_dat_name(name: &str) -> bool {
    name.len() > 4 && name[name.len() - 4..].eq_ignore_ascii_case(".dat")
}

fn open_error(kind: &str, detail: impl Into<String>) -> RecordError {
    RecordError {
        kind: kind.into(),
        detail: detail.into(),
    }
}

impl From<crate::paths::PathError> for RecordError {
    fn from(e: crate::paths::PathError) -> Self {
        open_error(e.kind(), e.to_string())
    }
}

/// Open a recording folder: Studio's (`recording.json`), Studio's older "seq" export
/// (`manifest.json`, whose frame names are confined to the folder), or a camera video
/// (no manifest: every `.DAT` sorted by name, no times).
pub fn open_recording(dir: &Path) -> Result<RecordingIndex, RecordError> {
    if !dir.is_dir() {
        return Err(open_error(
            "not_found",
            format!("{} is not a folder", dir.display()),
        ));
    }
    let manifest = read_manifest(dir).map_err(|e| open_error("bad_file", e.to_string()))?;
    let (files, t_ms, source, fps, sensor_material) = if let Some(m) = manifest {
        let files = (0..m.frames)
            .map(|i| crate::paths::confine(dir, &frame_file_name(i)))
            .collect::<Result<Vec<_>, _>>()?;
        let t = (m.t_ms.len() == m.frames).then_some(m.t_ms);
        (files, t, m.source, m.fps_nominal, m.sensor_material)
    } else if let Ok(bytes) = fs::read(dir.join("manifest.json")) {
        let m: crate::dat::SequenceManifest =
            serde_json::from_slice(&bytes).map_err(|e| open_error("bad_file", e.to_string()))?;
        let files = m
            .frames
            .iter()
            .map(|name| crate::paths::confine(dir, name))
            .collect::<Result<Vec<_>, _>>()?;
        (files, None, RecordingSource::Studio, m.fps, None)
    } else {
        let mut names: Vec<String> = fs::read_dir(dir)
            .map_err(|e| open_error("not_found", e.to_string()))?
            .flatten()
            .filter(|e| e.file_type().map(|t| t.is_file()).unwrap_or(false))
            .filter_map(|e| e.file_name().to_str().map(str::to_string))
            .filter(|n| is_dat_name(n))
            .collect();
        names.sort();
        let files = names
            .iter()
            .map(|n| crate::paths::confine(dir, n))
            .collect::<Result<Vec<_>, _>>()?;
        (files, None, RecordingSource::Camera, NOMINAL_FPS, None)
    };
    let first = files
        .first()
        .ok_or_else(|| open_error("empty", "The recording holds no frames."))?;
    let dat = read_dat_file(first)?;
    Ok(RecordingIndex {
        frames: files.len(),
        t_ms,
        width: dat.width,
        height: dat.height,
        source,
        fps_nominal: fps,
        sensor_material,
        files,
    })
}

fn read_dat_file(path: &Path) -> Result<ThermalDat, RecordError> {
    let bytes = fs::read(path).map_err(RecordError::from)?;
    ThermalDat::from_bytes(&bytes).map_err(|e| open_error("bad_file", e.to_string()))
}

/// Frame `index` of an opened recording, read from disk.
pub fn read_recording_frame(rec: &RecordingIndex, index: usize) -> Result<ThermalDat, RecordError> {
    let path = rec
        .files
        .get(index)
        .ok_or_else(|| open_error("out_of_range", format!("frame {index} of {}", rec.frames)))?;
    read_dat_file(path)
}

/// A `.DAT` frame in the live stream's `FrameRaw` wire layout (64-byte metadata + LE pixels),
/// so the player decodes recorded frames exactly like live ones.
#[must_use]
pub fn frame_raw_bytes(dat: &ThermalDat, index: u32, t_ms: Option<u32>) -> Vec<u8> {
    frame_from_dat(dat, index, t_ms).to_bytes()
}

/// A recorded `.DAT` frame as a `FrameRaw` (`timestamp_us` from its time, when it has one).
#[must_use]
pub fn frame_from_dat(dat: &ThermalDat, index: u32, t_ms: Option<u32>) -> FrameRaw {
    use crate::protocol::codec::{FrameRawMeta, SparseTempPoint};
    let (mut min_xy, mut max_xy) = ((0u16, 0u16), (0u16, 0u16));
    let (mut lo, mut hi) = (u16::MAX, 0u16);
    let w = dat.width.max(1);
    for (i, &p) in dat.pixels.iter().enumerate() {
        let xy = ((i % usize::from(w)) as u16, (i / usize::from(w)) as u16);
        if p < lo {
            lo = p;
            min_xy = xy;
        }
        if p > hi {
            hi = p;
            max_xy = xy;
        }
    }
    FrameRaw {
        meta: FrameRawMeta {
            frame_seq: index,
            timestamp_us: u64::from(t_ms.unwrap_or(0)) * 1000,
            rtc_epoch: 0,
            dropped_frames: 0,
            width: dat.width,
            height: dat.height,
            pixel_format: 0,
            gain_mode: 0,
            ffc_state: 0,
            flags: 0,
            min_raw: if dat.pixels.is_empty() { 0 } else { lo },
            max_raw: hi,
            min_x: min_xy.0,
            min_y: min_xy.1,
            max_x: max_xy.0,
            max_y: max_xy.1,
            spot_temp_c: dat.spot_temp,
            cal_offset: dat.cal_offset,
            cal_slope: dat.cal_slope,
            fpa_temp_c: 0.0,
            bat_pct: -1,
            usb_power: false,
            temp_point_count: 0,
        },
        pixels: dat.pixels.clone(),
        temp_points: dat
            .temp_points
            .iter()
            .map(|p| SparseTempPoint {
                x: p.x,
                y: p.y,
                raw: p.raw,
            })
            .collect(),
    }
}

/// Write frames `from..=to` of `rec` to `dest` as a recording of their own: frames renumbered
/// from `000000.DAT`, times rebased to 0, same source and sensor material.
pub fn write_trimmed_recording(
    rec: &RecordingIndex,
    from: usize,
    to: usize,
    dest: &Path,
) -> Result<PathBuf, RecordError> {
    if from > to || to >= rec.frames {
        return Err(open_error(
            "bad_range",
            format!("frames {from}–{to} of a {}-frame recording", rec.frames),
        ));
    }
    fs::create_dir_all(dest)?;
    for (n, src) in rec.files[from..=to].iter().enumerate() {
        fs::copy(src, dest.join(frame_file_name(n)))?;
    }
    let t_ms = rec.t_ms.as_ref().map_or_else(Vec::new, |t| {
        let base = t[from];
        t[from..=to].iter().map(|v| v - base).collect()
    });
    let manifest = RecordingManifest {
        schema_version: RECORDING_SCHEMA_VERSION,
        source: rec.source,
        frames: to - from + 1,
        fps_nominal: rec.fps_nominal,
        t_ms,
        sensor_material: rec.sensor_material,
    };
    let json = serde_json::to_vec(&manifest).map_err(|e| open_error("other", e.to_string()))?;
    write_atomic(&dest.join(RECORDING_MANIFEST), &json)?;
    Ok(dest.to_path_buf())
}

/// Copy a video folder from the camera's card into `dest`, one bounded read at a time,
/// skipping frames already copied. `progress(done, total)` is called before the first and
/// after every frame. Entry names are confined to `dest`.
pub fn fetch_card_folder<C, P>(
    card: &C,
    sd_dir: &str,
    dest: &Path,
    mut progress: P,
) -> Result<PathBuf, RecordError>
where
    C: crate::card::CardAccess + ?Sized,
    P: FnMut(usize, usize),
{
    let sd_dir = crate::paths::card_path(sd_dir)?;
    let card_err = |e: crate::card::CardError| open_error("card", e.to_string());
    let mut entries: Vec<_> = crate::card::list_card_dir(card, &sd_dir)
        .map_err(card_err)?
        .into_iter()
        .filter(|e| !e.is_dir && is_dat_name(&e.name))
        .collect();
    entries.sort_by(|a, b| a.name.cmp(&b.name));
    fs::create_dir_all(dest)?;
    let total = entries.len();
    progress(0, total);
    for (i, entry) in entries.iter().enumerate() {
        let local = crate::paths::confine(dest, &entry.name)?;
        let have = fs::metadata(&local).map(|m| m.len()).ok();
        if have != Some(u64::from(entry.size)) {
            let remote = crate::paths::card_path(&format!("{sd_dir}/{}", entry.name))?;
            let bytes = crate::card::read_card_file(card, &remote).map_err(card_err)?;
            write_atomic(&local, &bytes)?;
        }
        progress(i + 1, total);
    }
    Ok(dest.to_path_buf())
}

/// Why a recording ended.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum StopReason {
    User,
    Disconnected,
    WriteError,
    LowDisk,
}

/// Returned by `cmd_record_stop` and sent with `recording-stopped`.
#[derive(Debug, Clone, PartialEq, Serialize)]
pub struct RecordingInfo {
    pub path: String,
    pub frames: usize,
    pub duration_ms: u32,
    pub stopped: StopReason,
    /// The OS reason for `write_error`, the free space for `low_disk`.
    pub detail: Option<String>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct RecordError {
    pub kind: String,
    pub detail: String,
}

impl From<io::Error> for RecordError {
    fn from(e: io::Error) -> Self {
        Self {
            kind: classify_io(&e).as_str().into(),
            detail: e.to_string(),
        }
    }
}

/// Where frame files and the manifest go (a seam for failure tests).
pub trait FrameSink {
    fn write_frame(&mut self, path: &Path, bytes: &[u8]) -> io::Result<()>;
    fn write_manifest(&mut self, path: &Path, bytes: &[u8]) -> io::Result<()>;
}

/// Plain files: frames written directly, the manifest atomically.
pub struct StdSink;

impl FrameSink for StdSink {
    fn write_frame(&mut self, path: &Path, bytes: &[u8]) -> io::Result<()> {
        fs::write(path, bytes)
    }
    fn write_manifest(&mut self, path: &Path, bytes: &[u8]) -> io::Result<()> {
        write_atomic(path, bytes)
    }
}

#[derive(Debug, Clone, Copy)]
pub struct RecordingOptions {
    pub source: RecordingSource,
    pub sensor_material: Option<Material>,
    /// Time-lapse: keep at most one frame per this many milliseconds.
    pub min_interval_ms: Option<u32>,
}

impl RecordingOptions {
    #[must_use]
    pub fn studio(sensor_material: Option<Material>) -> Self {
        Self {
            source: RecordingSource::Studio,
            sensor_material,
            min_interval_ms: None,
        }
    }

    #[must_use]
    pub fn timelapse(interval_ms: u32, sensor_material: Option<Material>) -> Self {
        Self {
            source: RecordingSource::Timelapse,
            sensor_material,
            min_interval_ms: Some(interval_ms.max(1)),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PushOutcome {
    Written,
    /// Time-lapse: too soon after the last kept frame.
    Skipped,
    Stopped(StopReason),
}

pub type FreeSpace = Box<dyn FnMut() -> Option<u64> + Send>;

pub struct RecordingWriter<S: FrameSink = StdSink> {
    dir: PathBuf,
    opts: RecordingOptions,
    sink: S,
    free_space: FreeSpace,
    t_ms: Vec<u32>,
    last_manifest_t: u32,
    last_space_check_t: u32,
    frames_since_space_check: u32,
    stopped: Option<(StopReason, Option<String>)>,
}

impl<S: FrameSink> RecordingWriter<S> {
    /// Create `dir` and an empty manifest. Refused below [`MIN_FREE_TO_START`] free.
    pub fn start(
        dir: PathBuf,
        opts: RecordingOptions,
        sink: S,
        mut free_space: FreeSpace,
    ) -> Result<Self, RecordError> {
        if let Some(free) = free_space() {
            if free < MIN_FREE_TO_START {
                return Err(RecordError {
                    kind: "low_disk".into(),
                    detail: format!("{} MB free", free / (1024 * 1024)),
                });
            }
        }
        fs::create_dir_all(&dir)?;
        let mut w = Self {
            dir,
            opts,
            sink,
            free_space,
            t_ms: Vec::new(),
            last_manifest_t: 0,
            last_space_check_t: 0,
            frames_since_space_check: 0,
            stopped: None,
        };
        w.write_manifest()?;
        Ok(w)
    }

    #[must_use]
    pub fn dir(&self) -> &Path {
        &self.dir
    }

    #[must_use]
    pub fn sink(&self) -> &S {
        &self.sink
    }

    #[must_use]
    pub fn frames(&self) -> usize {
        self.t_ms.len()
    }

    #[must_use]
    pub fn source(&self) -> RecordingSource {
        self.opts.source
    }

    /// Heap memory the writer holds (the per-frame times).
    #[must_use]
    pub fn heap_bytes(&self) -> usize {
        self.t_ms.capacity() * std::mem::size_of::<u32>() + self.dir.as_os_str().len()
    }

    /// Why the writer stopped by itself, if it did.
    #[must_use]
    pub fn stopped(&self) -> Option<StopReason> {
        self.stopped.as_ref().map(|(r, _)| *r)
    }

    /// Write `frame`, taken `t_ms` milliseconds after the recording started.
    pub fn push(&mut self, frame: &FrameRaw, t_ms: u32) -> PushOutcome {
        if let Some((reason, _)) = &self.stopped {
            return PushOutcome::Stopped(*reason);
        }
        if let (Some(interval), Some(&last)) = (self.opts.min_interval_ms, self.t_ms.last()) {
            if t_ms.saturating_sub(last) < interval {
                return PushOutcome::Skipped;
            }
        }
        self.frames_since_space_check += 1;
        if self.t_ms.is_empty()
            || self.frames_since_space_check >= SPACE_CHECK_FRAMES
            || t_ms.saturating_sub(self.last_space_check_t) >= SPACE_CHECK_MS
        {
            self.frames_since_space_check = 0;
            self.last_space_check_t = t_ms;
            if let Some(free) = (self.free_space)() {
                if free < MIN_FREE_TO_CONTINUE {
                    return self.stop(
                        StopReason::LowDisk,
                        Some(format!("{} MB free", free / (1024 * 1024))),
                    );
                }
            }
        }
        let bytes = ThermalDat::from_frame(frame, DAT_COLOR_SCHEME, 0).to_bytes();
        let path = self.dir.join(frame_file_name(self.t_ms.len()));
        if let Err(e) = self.sink.write_frame(&path, &bytes) {
            let _ = fs::remove_file(&path);
            return self.stop(StopReason::WriteError, Some(e.to_string()));
        }
        self.t_ms.push(t_ms);
        if t_ms.saturating_sub(self.last_manifest_t) >= MANIFEST_REWRITE_MS {
            self.last_manifest_t = t_ms;
            if let Err(e) = self.write_manifest() {
                return self.stop(StopReason::WriteError, Some(e.to_string()));
            }
        }
        PushOutcome::Written
    }

    fn stop(&mut self, reason: StopReason, detail: Option<String>) -> PushOutcome {
        let _ = self.write_manifest();
        self.stopped = Some((reason, detail));
        PushOutcome::Stopped(reason)
    }

    fn fps_nominal(&self) -> f32 {
        if let Some(interval) = self.opts.min_interval_ms {
            return 1000.0 / interval as f32;
        }
        match (self.t_ms.first(), self.t_ms.last()) {
            (Some(&a), Some(&b)) if self.t_ms.len() > 1 && b > a => {
                (self.t_ms.len() - 1) as f32 * 1000.0 / (b - a) as f32
            }
            _ => NOMINAL_FPS,
        }
    }

    fn manifest_bytes(&self) -> Vec<u8> {
        let m = RecordingManifest {
            schema_version: RECORDING_SCHEMA_VERSION,
            source: self.opts.source,
            frames: self.t_ms.len(),
            fps_nominal: self.fps_nominal(),
            t_ms: self.t_ms.clone(),
            sensor_material: self.opts.sensor_material,
        };
        serde_json::to_vec(&m).unwrap_or_default()
    }

    fn write_manifest(&mut self) -> io::Result<()> {
        let bytes = self.manifest_bytes();
        self.sink
            .write_manifest(&self.dir.join(RECORDING_MANIFEST), &bytes)
    }

    /// Close the recording. A reason the writer already stopped for wins over `reason`.
    pub fn finish(mut self, reason: StopReason) -> RecordingInfo {
        let manifest = self.write_manifest();
        let (stopped, detail) = match self.stopped.take() {
            Some((r, d)) => (r, d),
            None => match manifest {
                Ok(()) => (reason, None),
                Err(e) => (StopReason::WriteError, Some(e.to_string())),
            },
        };
        let duration_ms = match (self.t_ms.first(), self.t_ms.last()) {
            (Some(&a), Some(&b)) => b.saturating_sub(a),
            _ => 0,
        };
        RecordingInfo {
            path: self.dir.to_string_lossy().into_owned(),
            frames: self.t_ms.len(),
            duration_ms,
            stopped,
            detail,
        }
    }
}
