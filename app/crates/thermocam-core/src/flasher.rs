//! Native Rust Teensy 4.1 HalfKay HID firmware updater (`16C0:0478`, 1088-byte packets)
//! with Intel HEX parsing (`ihex`), SHA-256 verification (`sha2`), and 134-baud legacy CDC fallback.

use std::collections::BTreeMap;
use std::time::Duration;

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

#[derive(Debug, Error)]
pub enum FlashError {
    #[error("Intel HEX parse error: {0}")]
    HexParse(String),
    #[error("firmware image is empty or outside Teensy 4.1 flash bounds")]
    InvalidImage,
    #[error("SHA-256 mismatch: expected {expected}, got {actual}")]
    ChecksumMismatch { expected: String, actual: String },
    #[error("HalfKay HID device (16C0:0478) not found")]
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

/// Parse an Intel HEX file for Teensy 4.1, verify optional SHA-256, and slice into 1024-byte blocks.
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
                let abs_base = upper_addr + u32::from(offset);
                for (i, &byte) in value.iter().enumerate() {
                    let abs = abs_base + i as u32;
                    let rel = abs.checked_sub(TEENSY41_FLASH_BASE).unwrap_or(abs);
                    if (rel as usize) >= TEENSY41_MAX_FLASH {
                        return Err(FlashError::InvalidImage);
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
            ihex::Record::EndOfFile => break,
            _ => {}
        }
    }

    if blocks.is_empty() {
        return Err(FlashError::InvalidImage);
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
#[must_use]
pub fn build_halfkay_packet(block_addr: u32, block: &[u8; TEENSY41_BLOCK_SIZE]) -> [u8; TEENSY41_REPORT_SIZE] {
    let mut pkt = [0u8; TEENSY41_REPORT_SIZE];
    // Teensy 4.x HalfKay header: 24-bit byte address LE in bytes 0..3, zeros in 3..64
    pkt[0] = (block_addr & 0xFF) as u8;
    pkt[1] = ((block_addr >> 8) & 0xFF) as u8;
    pkt[2] = ((block_addr >> 16) & 0xFF) as u8;
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

/// Abstract HalfKay HID device so flashing is 100% unit-tested without touching real hardware.
pub trait HalfKayDevice {
    fn write_report(&mut self, report: &[u8; TEENSY41_REPORT_SIZE]) -> Result<(), FlashError>;
}

/// Real USB HID HalfKay device backed by `hidapi`.
pub struct RealHalfKayDevice {
    dev: hidapi::HidDevice,
}

impl RealHalfKayDevice {
    pub fn open() -> Result<Self, FlashError> {
        let api = hidapi::HidApi::new().map_err(|e| FlashError::HidIo(e.to_string()))?;
        let dev = api
            .open(HALFKAY_VID, HALFKAY_PID)
            .map_err(|_| FlashError::DeviceNotFound)?;
        Ok(Self { dev })
    }
}

impl HalfKayDevice for RealHalfKayDevice {
    fn write_report(&mut self, report: &[u8; TEENSY41_REPORT_SIZE]) -> Result<(), FlashError> {
        // HID report ID 0 prefix required by hidapi on macOS/Windows/Linux
        let mut buf = [0u8; TEENSY41_REPORT_SIZE + 1];
        buf[1..].copy_from_slice(report);
        self.dev
            .write(&buf)
            .map_err(|e| FlashError::HidIo(e.to_string()))?;
        Ok(())
    }
}

/// Deterministic mock HID device recording every 1088-byte packet sent.
#[derive(Default)]
pub struct MockHalfKayDevice {
    pub packets: Vec<[u8; TEENSY41_REPORT_SIZE]>,
}

impl HalfKayDevice for MockHalfKayDevice {
    fn write_report(&mut self, report: &[u8; TEENSY41_REPORT_SIZE]) -> Result<(), FlashError> {
        self.packets.push(*report);
        Ok(())
    }
}

/// Flash all blocks of `image` to `device`, reporting progress `(done, total)`, then send HalfKay reboot.
pub fn flash_firmware<D, F>(
    device: &mut D,
    image: &FirmwareImage,
    mut on_progress: F,
) -> Result<(), FlashError>
where
    D: HalfKayDevice,
    F: FnMut(usize, usize),
{
    let total = image.blocks.len();
    for (idx, (&addr, block)) in image.blocks.iter().enumerate() {
        let pkt = build_halfkay_packet(addr, block);
        device.write_report(&pkt)?;
        on_progress(idx + 1, total);
    }
    let reboot = build_halfkay_reboot_packet();
    device.write_report(&reboot)?;
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
