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

/// Real USB HID HalfKay device backed by `hidapi`.
pub struct RealHalfKayDevice {
    dev: hidapi::HidDevice,
}

impl RealHalfKayDevice {
    /// Attempt a single immediate open of HalfKay `16C0:0478`.
    pub fn open() -> Result<Self, FlashError> {
        let api = hidapi::HidApi::new().map_err(|e| FlashError::HidIo(e.to_string()))?;
        let dev = api
            .open(HALFKAY_VID, HALFKAY_PID)
            .map_err(|_| FlashError::DeviceNotFound)?;
        Ok(Self { dev })
    }

    /// Poll for HalfKay `16C0:0478` to enumerate over USB up to `timeout` (e.g. 10 s after reboot).
    pub fn open_with_timeout(
        timeout: Duration,
        poll_interval: Duration,
    ) -> Result<Self, FlashError> {
        wait_for_device(timeout, poll_interval, Self::open)
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

/// Legacy v3.0.x bootloader trigger: open CDC serial at 134 baud (8N1) and close it.
pub fn trigger_legacy_134_baud(port_name: &str) -> Result<(), FlashError> {
    let port = serialport::new(port_name, 134)
        .timeout(Duration::from_millis(100))
        .open()
        .map_err(|e| FlashError::SerialTrigger(e.to_string()))?;
    drop(port);
    Ok(())
}
