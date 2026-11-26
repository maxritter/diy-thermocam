//! Native Rust Teensy 4.1 HalfKay HID firmware updater (`16C0:0478`, 1088-byte packets)
//! with Intel HEX parsing (`ihex`), Teensy 4.1 FlexSPI address/header validation,
//! SHA-256 verification (`sha2`), enumeration polling, per-block retry, and 134-baud CDC fallback.

use std::collections::BTreeMap;
use std::thread;
use std::time::{Duration, Instant};

use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use thiserror::Error;

pub const HALFKAY_VID: u16 = 0x16C0;
pub const HALFKAY_PID: u16 = 0x0478;
/// Teensy 4.1 HalfKay block size (1024 bytes of flash data per HID write).
pub const TEENSY41_BLOCK_SIZE: usize = 1024;
/// Teensy 4.1 HalfKay HID report size: 64-byte header + 1024-byte block = 1088 bytes.
pub const TEENSY41_REPORT_SIZE: usize = 1088;
/// Teensy 4.1 flash base address (FlexSPI NOR).
pub const TEENSY41_FLASH_BASE: u32 = 0x6000_0000;
/// Teensy 4.1 maximum flash size (8 MiB).
pub const TEENSY41_MAX_FLASH: usize = 8 * 1024 * 1024;
/// i.MX RT1062 FlexSPI Configuration Block (FCB) magic at flash offset `0x0000` ("FCFB").
pub const TEENSY4_FCB_MAGIC: [u8; 4] = *b"FCFB";

#[derive(Debug, Error)]
pub enum FlashError {
    #[error("Intel HEX parse error: {0}")]
    HexParse(String),
    #[error("firmware image is truncated (missing Intel HEX End-Of-File record)")]
    TruncatedHex,
    #[error(
        "address 0x{0:08X} is outside Teensy 4.1 FlexSPI flash bounds (0x60000000..0x60800000)"
    )]
    AddressOutOfBounds(u32),
    #[error("invalid Teensy 4.1 boot header (expected 'FCFB' FlexSPI config block at 0x60000000)")]
    InvalidBootHeader,
    #[error("firmware image is empty or outside Teensy 4.1 flash bounds")]
    InvalidImage,
    #[error("SHA-256 mismatch: expected {expected}, got {actual}")]
    ChecksumMismatch { expected: String, actual: String },
    #[error("HalfKay HID device (16C0:0478) not found after waiting")]
    DeviceNotFound,
    #[error("no permission to open the HalfKay HID device: {0}")]
    PermissionDenied(String),
    #[error("HID I/O error: {0}")]
    HidIo(String),
    #[error("serial 134-baud trigger failed: {0}")]
    SerialTrigger(String),
}

/// Verified Teensy 4.1 firmware image parsed from Intel HEX.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FirmwareImage {
    pub sha256_hex: String,
    pub total_bytes: usize,
    pub block_count: usize,
    pub min_addr: u32,
    pub max_addr: u32,
    /// Map of 1024-byte aligned relative block offset -> 1024-byte flash block (padded with `0xFF`).
    #[serde(skip)]
    pub blocks: BTreeMap<u32, [u8; TEENSY41_BLOCK_SIZE]>,
}

/// Parse an Intel HEX file for Teensy 4.1, verify optional SHA-256, validate FlexSPI NOR address
/// bounds (`0x6000_0000..0x6080_0000`), EOF completeness, and i.MX RT1062 `FCFB` boot header.
pub fn parse_teensy41_hex(
    hex_text: &str,
    expected_sha256: Option<&str>,
) -> Result<FirmwareImage, FlashError> {
    let mut hasher = Sha256::new();
    hasher.update(hex_text.as_bytes());
    let actual_hash = format!("{:x}", hasher.finalize());
    if let Some(exp) = expected_sha256 {
        if !exp.eq_ignore_ascii_case(&actual_hash) {
            return Err(FlashError::ChecksumMismatch {
                expected: exp.to_string(),
                actual: actual_hash,
            });
        }
    }

    let reader = ihex::Reader::new(hex_text);
    let mut upper_addr: u32 = 0;
    let mut blocks: BTreeMap<u32, [u8; TEENSY41_BLOCK_SIZE]> = BTreeMap::new();
    let mut min_addr = u32::MAX;
    let mut max_addr = 0u32;
    let mut total_bytes = 0usize;
    let mut saw_eof = false;

    for record in reader {
        let rec = record.map_err(|e| FlashError::HexParse(e.to_string()))?;
        match rec {
            ihex::Record::ExtendedLinearAddress(hi) => {
                upper_addr = u32::from(hi) << 16;
            }
            ihex::Record::ExtendedSegmentAddress(seg) => {
                upper_addr = u32::from(seg) << 4;
            }
            ihex::Record::Data { offset, value } => {
                let abs_base = upper_addr
                    .checked_add(u32::from(offset))
                    .ok_or(FlashError::AddressOutOfBounds(u32::MAX))?;
                for (i, &byte) in value.iter().enumerate() {
                    let abs = abs_base
                        .checked_add(i as u32)
                        .ok_or(FlashError::AddressOutOfBounds(u32::MAX))?;
                    let rel = abs
                        .checked_sub(TEENSY41_FLASH_BASE)
                        .ok_or(FlashError::AddressOutOfBounds(abs))?;
                    if (rel as usize) >= TEENSY41_MAX_FLASH {
                        return Err(FlashError::AddressOutOfBounds(abs));
                    }
                    let block_addr = rel & !(TEENSY41_BLOCK_SIZE as u32 - 1);
                    let in_block = (rel as usize) & (TEENSY41_BLOCK_SIZE - 1);
                    let block = blocks
                        .entry(block_addr)
                        .or_insert([0xFFu8; TEENSY41_BLOCK_SIZE]);
                    block[in_block] = byte;
                    min_addr = min_addr.min(rel);
                    max_addr = max_addr.max(rel);
                    total_bytes += 1;
                }
            }
            ihex::Record::EndOfFile => {
                saw_eof = true;
                break;
            }
            _ => {}
        }
    }

    if !saw_eof {
        return Err(FlashError::TruncatedHex);
    }
    if blocks.is_empty() {
        return Err(FlashError::InvalidImage);
    }
    let first_block = blocks.get(&0).ok_or(FlashError::InvalidBootHeader)?;
    if first_block[0..4] != TEENSY4_FCB_MAGIC {
        return Err(FlashError::InvalidBootHeader);
    }

    Ok(FirmwareImage {
        sha256_hex: actual_hash,
        total_bytes,
        block_count: blocks.len(),
        min_addr,
        max_addr,
        blocks,
    })
}

/// Build a 1088-byte Teensy 4.1 HalfKay HID write packet (64-byte header + 1024-byte block).
///
/// Verified against PJRC `teensy_loader_cli.c`:
/// - `buf[0..3]`: 24-bit little-endian byte offset from `0x6000_0000`
/// - `buf[3..64]`: 61 zero bytes (`memset(buf + 3, 0, 61)`)
/// - `buf[64..1088]`: 1024-byte flash block (unwritten flash bytes padded with `0xFF`)
#[must_use]
pub fn build_halfkay_packet(
    block_addr: u32,
    block: &[u8; TEENSY41_BLOCK_SIZE],
) -> [u8; TEENSY41_REPORT_SIZE] {
    let mut pkt = [0u8; TEENSY41_REPORT_SIZE];
    pkt[0] = (block_addr & 0xFF) as u8;
    pkt[1] = ((block_addr >> 8) & 0xFF) as u8;
    pkt[2] = ((block_addr >> 16) & 0xFF) as u8;
    // pkt[3..64] remains 0x00 per Teensy 4 HalfKay spec
    pkt[64..].copy_from_slice(block);
    pkt
}

/// Build the Teensy 4.1 HalfKay reboot command packet (`0xFF 0xFF 0xFF` + zeros).
#[must_use]
pub fn build_halfkay_reboot_packet() -> [u8; TEENSY41_REPORT_SIZE] {
    let mut pkt = [0u8; TEENSY41_REPORT_SIZE];
    pkt[0] = 0xFF;
    pkt[1] = 0xFF;
    pkt[2] = 0xFF;
    pkt
}

/// Retry and timing configuration for HalfKay flashing.
#[derive(Debug, Clone, Copy)]
pub struct FlashOptions {
    /// Maximum attempts per block (including initial attempt).
    pub max_attempts: usize,
    /// Backoff delay between retries on block 0 (when Teensy 4 erases FlexSPI NOR flash).
    pub first_block_retry_delay: Duration,
    /// Backoff delay between retries on subsequent blocks.
    pub block_retry_delay: Duration,
    /// Extra attempts specifically granted to block 0 flash erase.
    pub first_block_extra_attempts: usize,
}

impl Default for FlashOptions {
    fn default() -> Self {
        Self {
            max_attempts: 8,
            first_block_retry_delay: Duration::from_millis(500),
            block_retry_delay: Duration::from_millis(100),
            first_block_extra_attempts: 8,
        }
    }
}

/// Abstract HalfKay HID device so flashing is 100% unit-tested without touching real hardware.
pub trait HalfKayDevice {
    fn write_report(&mut self, report: &[u8; TEENSY41_REPORT_SIZE]) -> Result<(), FlashError>;
}

type HidJob = Box<dyn FnOnce(&mut Option<hidapi::HidApi>) + Send>;

/// Runs `job` on the one thread that owns this process's `HidApi`.
///
/// `hidapi` keeps one global HID manager per process (on macOS an `IOHIDManager` scheduled on
/// the creating thread's run loop). Creating and dropping an `HidApi` per call from short-lived
/// worker threads tore that manager down under concurrent callers and crashed the app
/// (`EXC_BREAKPOINT` in `CFRunLoopAddSource`). One long-lived thread holds one `HidApi`,
/// created on first use and never dropped, and serves every listing and open in turn.
fn with_hid<R: Send + 'static>(
    job: impl FnOnce(&mut hidapi::HidApi) -> Result<R, FlashError> + Send + 'static,
) -> Result<R, FlashError> {
    use std::sync::mpsc;
    use std::sync::{Mutex, OnceLock};

    static QUEUE: OnceLock<Mutex<mpsc::Sender<HidJob>>> = OnceLock::new();
    let queue = QUEUE.get_or_init(|| {
        let (tx, rx) = mpsc::channel::<HidJob>();
        std::thread::Builder::new()
            .name("hid".into())
            .spawn(move || {
                let mut api: Option<hidapi::HidApi> = None;
                for job in rx {
                    job(&mut api);
                }
            })
            .expect("spawn the HID thread");
        Mutex::new(tx)
    });

    let (reply_tx, reply_rx) = std::sync::mpsc::channel();
    let wrapped: HidJob = Box::new(move |slot: &mut Option<hidapi::HidApi>| {
        let result = (|| {
            if slot.is_none() {
                *slot = Some(hidapi::HidApi::new().map_err(|e| FlashError::HidIo(e.to_string()))?);
            }
            let api = slot.as_mut().expect("HidApi was just created");
            api.refresh_devices()
                .map_err(|e| FlashError::HidIo(e.to_string()))?;
            job(api)
        })();
        let _ = reply_tx.send(result);
    });
    queue
        .lock()
        .map_err(|_| FlashError::HidIo("HID thread is unavailable".into()))?
        .send(wrapped)
        .map_err(|_| FlashError::HidIo("HID thread is unavailable".into()))?;
    reply_rx
        .recv()
        .map_err(|_| FlashError::HidIo("HID thread stopped".into()))?
}

/// Real USB HID HalfKay device backed by `hidapi`.
pub struct RealHalfKayDevice {
    dev: hidapi::HidDevice,
}

impl RealHalfKayDevice {
    /// Attempt a single immediate open of HalfKay `16C0:0478`.
    pub fn open() -> Result<Self, FlashError> {
        with_hid(|api| {
            api.open(HALFKAY_VID, HALFKAY_PID)
                .map(|dev| Self { dev })
                .map_err(|e| classify_hid_open_error(&e.to_string()))
        })
    }

    /// Poll for HalfKay `16C0:0478` to enumerate over USB up to `timeout` (e.g. 10 s after reboot).
    pub fn open_with_timeout(
        timeout: Duration,
        poll_interval: Duration,
    ) -> Result<Self, FlashError> {
        wait_for_device(timeout, poll_interval, Self::open)
    }
}

/// The USB HID devices this computer sees, as `(vendor id, product id)`. Listing only: no
/// device is opened.
pub trait HidBus {
    fn device_ids(&self) -> Result<Vec<(u16, u16)>, FlashError>;
}

/// The real HID bus through `hidapi`'s device list.
pub struct RealHidBus;

impl HidBus for RealHidBus {
    fn device_ids(&self) -> Result<Vec<(u16, u16)>, FlashError> {
        with_hid(|api| {
            Ok(api
                .device_list()
                .map(|d| (d.vendor_id(), d.product_id()))
                .collect())
        })
    }
}

impl HidBus for [(u16, u16)] {
    fn device_ids(&self) -> Result<Vec<(u16, u16)>, FlashError> {
        Ok(self.to_vec())
    }
}

impl HidBus for Vec<(u16, u16)> {
    fn device_ids(&self) -> Result<Vec<(u16, u16)>, FlashError> {
        Ok(self.clone())
    }
}

/// A mock HalfKay device is on its own bus.
impl HidBus for MockHalfKayDevice {
    fn device_ids(&self) -> Result<Vec<(u16, u16)>, FlashError> {
        Ok(vec![(HALFKAY_VID, HALFKAY_PID)])
    }
}

/// A Thermocam waits in update mode: the HalfKay bootloader (`16C0:0478`) is on the bus.
/// A bus that cannot be listed counts as "not in update mode".
pub fn update_mode_present<B: HidBus + ?Sized>(bus: &B) -> bool {
    bus.device_ids()
        .map(|ids| ids.contains(&(HALFKAY_VID, HALFKAY_PID)))
        .unwrap_or(false)
}

/// An `hidapi` open failure: a permission problem (Linux udev, Windows access) or simply absent.
#[must_use]
pub fn classify_hid_open_error(message: &str) -> FlashError {
    let lower = message.to_ascii_lowercase();
    if lower.contains("permission denied") || lower.contains("access is denied") {
        FlashError::PermissionDenied(message.to_string())
    } else {
        FlashError::DeviceNotFound
    }
}

/// Generic enumeration polling loop (unit-testable without real USB hardware).
pub fn wait_for_device<D, F>(
    timeout: Duration,
    poll_interval: Duration,
    mut try_open: F,
) -> Result<D, FlashError>
where
    F: FnMut() -> Result<D, FlashError>,
{
    let deadline = Instant::now() + timeout;
    loop {
        match try_open() {
            Ok(dev) => return Ok(dev),
            Err(FlashError::DeviceNotFound) => {
                let now = Instant::now();
                if now >= deadline {
                    return Err(FlashError::DeviceNotFound);
                }
                let rem = deadline.saturating_duration_since(now);
                thread::sleep(poll_interval.min(rem));
            }
            Err(other) => return Err(other),
        }
    }
}

impl HalfKayDevice for RealHalfKayDevice {
    fn write_report(&mut self, report: &[u8; TEENSY41_REPORT_SIZE]) -> Result<(), FlashError> {
        // HID report ID 0 prefix required by hidapi on macOS/Windows/Linux
        let mut buf = [0u8; TEENSY41_REPORT_SIZE + 1];
        buf[1..].copy_from_slice(report);
        let written = self
            .dev
            .write(&buf)
            .map_err(|e| FlashError::HidIo(e.to_string()))?;
        if written < TEENSY41_REPORT_SIZE {
            return Err(FlashError::HidIo(format!(
                "short HID write ({written} < {TEENSY41_REPORT_SIZE} bytes)"
            )));
        }
        Ok(())
    }
}

/// Deterministic mock HID device recording every 1088-byte packet sent, with optional transient failure injection.
#[derive(Default)]
pub struct MockHalfKayDevice {
    pub packets: Vec<[u8; TEENSY41_REPORT_SIZE]>,
    /// Map of 0-based write call index (or block index) -> remaining transient failures to simulate.
    pub fail_block_attempts: BTreeMap<usize, usize>,
    current_block_idx: usize,
}

impl HalfKayDevice for MockHalfKayDevice {
    fn write_report(&mut self, report: &[u8; TEENSY41_REPORT_SIZE]) -> Result<(), FlashError> {
        let idx = self.current_block_idx;
        if let Some(rem) = self.fail_block_attempts.get_mut(&idx) {
            if *rem > 0 {
                *rem -= 1;
                return Err(FlashError::HidIo(format!("simulated busy on block {idx}")));
            }
        }
        self.packets.push(*report);
        self.current_block_idx += 1;
        Ok(())
    }
}

fn write_with_retry<D: HalfKayDevice>(
    device: &mut D,
    pkt: &[u8; TEENSY41_REPORT_SIZE],
    attempts: usize,
    delay: Duration,
) -> Result<(), FlashError> {
    let max_tries = attempts.max(1);
    let mut last_err = None;
    for attempt in 0..max_tries {
        match device.write_report(pkt) {
            Ok(()) => return Ok(()),
            Err(e) => {
                last_err = Some(e);
                if attempt + 1 < max_tries && !delay.is_zero() {
                    thread::sleep(delay);
                }
            }
        }
    }
    Err(last_err.unwrap_or_else(|| FlashError::HidIo("write failed".into())))
}

/// Flash all blocks of `image` to `device` with default retry/backoff policy, then send HalfKay reboot.
pub fn flash_firmware<D, F>(
    device: &mut D,
    image: &FirmwareImage,
    on_progress: F,
) -> Result<(), FlashError>
where
    D: HalfKayDevice,
    F: FnMut(usize, usize),
{
    flash_firmware_with_options(device, image, FlashOptions::default(), on_progress)
}

/// Flash all blocks of `image` to `device` using explicit `FlashOptions` (per-block retries + block-0 erase backoff).
pub fn flash_firmware_with_options<D, F>(
    device: &mut D,
    image: &FirmwareImage,
    opts: FlashOptions,
    mut on_progress: F,
) -> Result<(), FlashError>
where
    D: HalfKayDevice,
    F: FnMut(usize, usize),
{
    let total = image.blocks.len();
    for (idx, (&addr, block)) in image.blocks.iter().enumerate() {
        let pkt = build_halfkay_packet(addr, block);
        let is_first = idx == 0;
        let attempts = if is_first {
            opts.max_attempts + opts.first_block_extra_attempts
        } else {
            opts.max_attempts
        };
        let delay = if is_first {
            opts.first_block_retry_delay
        } else {
            opts.block_retry_delay
        };
        write_with_retry(device, &pkt, attempts, delay)?;
        on_progress(idx + 1, total);
    }
    let reboot = build_halfkay_reboot_packet();
    write_with_retry(device, &reboot, opts.max_attempts, opts.block_retry_delay)?;
    Ok(())
}

/// One `flash-progress` event, serialised as `{ "stage": "...", ... }`.
#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
#[serde(tag = "stage", rename_all = "snake_case")]
pub enum FlashEvent {
    Verified {
        sha256: String,
    },
    WaitingBootloader,
    /// The camera has not switched to update mode within the wait (10 s in Studio).
    WaitingTimeout,
    Flashing {
        done: usize,
        total: usize,
    },
    Rebooted {
        sha256: String,
    },
    Error {
        kind: FlashFailureKind,
        detail: String,
    },
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum FlashFailureKind {
    /// The update device appeared but may not be opened.
    Permission,
    /// Writing a block failed part-way.
    Write,
    /// The camera never switched to update mode.
    NotFound,
    /// The chosen file is not a valid Teensy 4.1 firmware (or its checksum is wrong).
    BadFile,
}

/// `{ kind, detail }` rejection of a firmware update.
#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct FlashFailure {
    pub kind: FlashFailureKind,
    pub detail: String,
}

impl From<FlashError> for FlashFailure {
    fn from(e: FlashError) -> Self {
        let kind = match &e {
            FlashError::HexParse(_)
            | FlashError::TruncatedHex
            | FlashError::AddressOutOfBounds(_)
            | FlashError::InvalidBootHeader
            | FlashError::InvalidImage
            | FlashError::ChecksumMismatch { .. } => FlashFailureKind::BadFile,
            FlashError::DeviceNotFound | FlashError::SerialTrigger(_) => FlashFailureKind::NotFound,
            FlashError::PermissionDenied(_) => FlashFailureKind::Permission,
            FlashError::HidIo(_) => FlashFailureKind::Write,
        };
        Self {
            kind,
            detail: e.to_string(),
        }
    }
}

#[derive(Debug, Clone, Copy)]
pub struct FlashUpdateOptions {
    /// How long to wait for the camera to switch to update mode.
    pub wait_timeout: Duration,
    pub poll_interval: Duration,
    pub flash: FlashOptions,
}

impl Default for FlashUpdateOptions {
    fn default() -> Self {
        Self {
            wait_timeout: Duration::from_secs(10),
            poll_interval: Duration::from_millis(100),
            flash: FlashOptions::default(),
        }
    }
}

/// The whole update: verify the file (before touching anything), run `before_wait` (ask the
/// camera to reboot into update mode), wait for the update device, flash, reboot.
/// Every stage is reported through `emit`; a failure is emitted as `stage: "error"` and returned.
pub fn run_flash_update<D, O, B, E>(
    hex_text: &str,
    expected_sha256: Option<&str>,
    opts: FlashUpdateOptions,
    before_wait: B,
    mut open_device: O,
    mut emit: E,
) -> Result<String, FlashFailure>
where
    D: HalfKayDevice,
    O: FnMut() -> Result<D, FlashError>,
    B: FnOnce(),
    E: FnMut(FlashEvent),
{
    let fail = |emit: &mut E, f: FlashFailure| -> FlashFailure {
        emit(FlashEvent::Error {
            kind: f.kind,
            detail: f.detail.clone(),
        });
        f
    };
    let img = match parse_teensy41_hex(hex_text, expected_sha256) {
        Ok(img) => img,
        Err(e) => return Err(fail(&mut emit, e.into())),
    };
    emit(FlashEvent::Verified {
        sha256: img.sha256_hex.clone(),
    });
    before_wait();
    emit(FlashEvent::WaitingBootloader);

    let deadline = Instant::now() + opts.wait_timeout;
    let mut device = loop {
        match open_device() {
            Ok(d) => break d,
            Err(e @ (FlashError::DeviceNotFound | FlashError::PermissionDenied(_))) => {
                let now = Instant::now();
                if now >= deadline {
                    if matches!(e, FlashError::DeviceNotFound) {
                        emit(FlashEvent::WaitingTimeout);
                    }
                    return Err(fail(&mut emit, e.into()));
                }
                thread::sleep(opts.poll_interval.min(deadline - now));
            }
            Err(other) => return Err(fail(&mut emit, other.into())),
        }
    };

    let flashed = flash_firmware_with_options(&mut device, &img, opts.flash, |done, total| {
        emit(FlashEvent::Flashing { done, total });
    });
    if let Err(e) = flashed {
        return Err(fail(&mut emit, e.into()));
    }
    emit(FlashEvent::Rebooted {
        sha256: img.sha256_hex.clone(),
    });
    Ok(img.sha256_hex)
}

/// Legacy v3.0.x bootloader trigger: open CDC serial at 134 baud (8N1) and close it.
pub fn trigger_legacy_134_baud(port_name: &str) -> Result<(), FlashError> {
    let port = serialport::new(port_name, 134)
        .timeout(Duration::from_millis(100))
        .open()
        .map_err(|e| FlashError::SerialTrigger(e.to_string()))?;
    drop(port);
    Ok(())
}
