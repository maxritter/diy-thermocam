//! DIY-Thermocam USB Protocol v2.0 implementation (`docs/protocol-v2.md`).

pub mod codec;
pub mod crc;
pub mod messages;
pub mod parser;
pub mod transport;

use serde::{Deserialize, Serialize};
use thiserror::Error;

/// Sync magic `"TC2\0"`.
pub const MAGIC: [u8; 4] = [0x54, 0x43, 0x32, 0x00];
/// Protocol major version `2`.
pub const PROTO_MAJOR: u8 = 2;
/// Protocol minor version `0`.
pub const PROTO_MINOR: u8 = 0;
/// Fixed header size in bytes.
pub const HEADER_LEN: usize = 16;
/// Fixed CRC-32 trailer size in bytes.
pub const TRAILER_LEN: usize = 4;
/// Maximum payload length accepted on the wire (163,840 B).
pub const MAX_PAYLOAD_LEN: usize = 163_840;
/// Safety magic for `ENTER_BOOTLOADER_REQ`.
pub const BOOTLOADER_MAGIC: u32 = 0xB007_10AD;
/// Teensy 4.1 PJRC USB VID.
pub const TEENSY_USB_VID: u16 = 0x16C0;
/// PJRC CDC serial PID (`usb=serial`).
pub const TEENSY_CDC_PID: u16 = 0x0483;
/// PJRC CDC+MTP PID (`usb=serialmtp` during transition).
pub const TEENSY_CDC_MTP_PID: u16 = 0x04D5;

/// Packet flag bits (§2.2).
pub mod flags {
    pub const ACK_REQ: u8 = 0x01;
    pub const ERROR: u8 = 0x02;
    pub const MORE_CHUNKS: u8 = 0x04;
    pub const COMPRESSED: u8 = 0x08;
}

/// Capability bitmask bits returned by `HELLO_RESP` (§4.2).
pub mod capabilities {
    pub const RAW_STREAM: u32 = 0x0001;
    pub const DISPLAY_STREAM: u32 = 0x0002;
    pub const SD_FS: u32 = 0x0004;
    pub const PERF_TELEMETRY: u32 = 0x0008;
    pub const BOOTLOADER: u32 = 0x0010;
    pub const LEPTON_RAD_PARAMS: u32 = 0x0020;
    pub const GAIN_MODE: u32 = 0x0040;
    pub const HOT_COLD: u32 = 0x0080;
    pub const RTC_SYNC: u32 = 0x0100;
    pub const KEEP_LCD_ON: u32 = 0x0200;
}

/// Protocol v2 message type IDs (§4.1).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
#[repr(u8)]
pub enum MsgType {
    HelloReq = 0x01,
    GoodbyeReq = 0x02,
    PingReq = 0x03,
    StreamCtrlReq = 0x10,
    GetConfigReq = 0x11,
    SetConfigReq = 0x12,
    SetTimeReq = 0x13,
    TriggerFfcReq = 0x14,
    GetTempPointsReq = 0x15,
    SetTempPointsReq = 0x16,
    SaveCaptureReq = 0x17,
    GetDiagnosticsReq = 0x18,
    GetPerfReq = 0x19,
    GetRadParamsReq = 0x1A,
    SetRadParamsReq = 0x1B,

    HelloResp = 0x41,
    GoodbyeResp = 0x42,
    PongResp = 0x43,
    StreamCtrlResp = 0x50,
    GetConfigResp = 0x51,
    SetConfigResp = 0x52,
    SetTimeResp = 0x53,
    TriggerFfcResp = 0x54,
    GetTempPointsResp = 0x55,
    SetTempPointsResp = 0x56,
    SaveCaptureResp = 0x57,
    GetDiagnosticsResp = 0x58,
    GetPerfResp = 0x59,
    GetRadParamsResp = 0x5A,
    SetRadParamsResp = 0x5B,

    FrameRaw = 0x80,
    FrameRendered = 0x81,

    EvtButton = 0xC0,
    EvtTouch = 0xC1,
    EvtBattery = 0xC2,
    EvtFfc = 0xC3,
    EvtSessionExit = 0xC4,
    EvtPerf = 0xC5,

    FsInfoReq = 0xE0,
    FsListReq = 0xE1,
    FsStatReq = 0xE2,
    FsReadReq = 0xE3,
    FsDeleteReq = 0xE4,
    FsAbortReq = 0xE5,

    FsInfoResp = 0xE8,
    FsListResp = 0xE9,
    FsStatResp = 0xEA,
    FsReadData = 0xEB,
    FsDeleteResp = 0xEC,
    FsAbortResp = 0xED,

    EnterBootloaderReq = 0xF0,
    EnterBootloaderResp = 0xF1,
    GenericErrorResp = 0xFF,
}

impl TryFrom<u8> for MsgType {
    type Error = ProtocolError;

    fn try_from(value: u8) -> Result<Self, Self::Error> {
        match value {
            0x01 => Ok(Self::HelloReq),
            0x02 => Ok(Self::GoodbyeReq),
            0x03 => Ok(Self::PingReq),
            0x10 => Ok(Self::StreamCtrlReq),
            0x11 => Ok(Self::GetConfigReq),
            0x12 => Ok(Self::SetConfigReq),
            0x13 => Ok(Self::SetTimeReq),
            0x14 => Ok(Self::TriggerFfcReq),
            0x15 => Ok(Self::GetTempPointsReq),
            0x16 => Ok(Self::SetTempPointsReq),
            0x17 => Ok(Self::SaveCaptureReq),
            0x18 => Ok(Self::GetDiagnosticsReq),
            0x19 => Ok(Self::GetPerfReq),
            0x1A => Ok(Self::GetRadParamsReq),
            0x1B => Ok(Self::SetRadParamsReq),
            0x41 => Ok(Self::HelloResp),
            0x42 => Ok(Self::GoodbyeResp),
            0x43 => Ok(Self::PongResp),
            0x50 => Ok(Self::StreamCtrlResp),
            0x51 => Ok(Self::GetConfigResp),
            0x52 => Ok(Self::SetConfigResp),
            0x53 => Ok(Self::SetTimeResp),
            0x54 => Ok(Self::TriggerFfcResp),
            0x55 => Ok(Self::GetTempPointsResp),
            0x56 => Ok(Self::SetTempPointsResp),
            0x57 => Ok(Self::SaveCaptureResp),
            0x58 => Ok(Self::GetDiagnosticsResp),
            0x59 => Ok(Self::GetPerfResp),
            0x5A => Ok(Self::GetRadParamsResp),
            0x5B => Ok(Self::SetRadParamsResp),
            0x80 => Ok(Self::FrameRaw),
            0x81 => Ok(Self::FrameRendered),
            0xC0 => Ok(Self::EvtButton),
            0xC1 => Ok(Self::EvtTouch),
            0xC2 => Ok(Self::EvtBattery),
            0xC3 => Ok(Self::EvtFfc),
            0xC4 => Ok(Self::EvtSessionExit),
            0xC5 => Ok(Self::EvtPerf),
            0xE0 => Ok(Self::FsInfoReq),
            0xE1 => Ok(Self::FsListReq),
            0xE2 => Ok(Self::FsStatReq),
            0xE3 => Ok(Self::FsReadReq),
            0xE4 => Ok(Self::FsDeleteReq),
            0xE5 => Ok(Self::FsAbortReq),
            0xE8 => Ok(Self::FsInfoResp),
            0xE9 => Ok(Self::FsListResp),
            0xEA => Ok(Self::FsStatResp),
            0xEB => Ok(Self::FsReadData),
            0xEC => Ok(Self::FsDeleteResp),
            0xED => Ok(Self::FsAbortResp),
            0xF0 => Ok(Self::EnterBootloaderReq),
            0xF1 => Ok(Self::EnterBootloaderResp),
            0xFF => Ok(Self::GenericErrorResp),
            other => Err(ProtocolError::UnknownMsgType(other)),
        }
    }
}

/// Protocol v2 error status codes (§3.2).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
#[repr(u16)]
pub enum StatusCode {
    Ok = 0x0000,
    ErrBadCrc = 0x0001,
    ErrUnknownMsg = 0x0002,
    ErrInvalidArg = 0x0003,
    ErrBusy = 0x0004,
    ErrIo = 0x0005,
    ErrNotFound = 0x0006,
    ErrForbidden = 0x0007,
    ErrUnsupported = 0x0008,
    ErrBadMagic = 0x0009,
    ErrNoSd = 0x000A,
}

impl StatusCode {
    #[must_use]
    pub fn from_u16(value: u16) -> Option<Self> {
        match value {
            0x0000 => Some(Self::Ok),
            0x0001 => Some(Self::ErrBadCrc),
            0x0002 => Some(Self::ErrUnknownMsg),
            0x0003 => Some(Self::ErrInvalidArg),
            0x0004 => Some(Self::ErrBusy),
            0x0005 => Some(Self::ErrIo),
            0x0006 => Some(Self::ErrNotFound),
            0x0007 => Some(Self::ErrForbidden),
            0x0008 => Some(Self::ErrUnsupported),
            0x0009 => Some(Self::ErrBadMagic),
            0x000A => Some(Self::ErrNoSd),
            _ => None,
        }
    }
}

/// Typed protocol and transport errors.
#[derive(Debug, Error)]
pub enum ProtocolError {
    #[error("unknown message type 0x{0:02X}")]
    UnknownMsgType(u8),
    #[error("payload length {0} exceeds MAX_PAYLOAD_LEN ({MAX_PAYLOAD_LEN})")]
    PayloadTooLarge(usize),
    #[error("truncated payload: expected {expected} bytes, got {actual}")]
    TruncatedPayload { expected: usize, actual: usize },
    #[error("invalid field value for {field}: {detail}")]
    InvalidField { field: &'static str, detail: String },
    #[error("CRC-32 mismatch: expected 0x{expected:08X}, got 0x{actual:08X}")]
    BadCrc { expected: u32, actual: u32 },
    #[error("header CRC-8 mismatch")]
    BadHeaderCrc,
    #[error("device returned error {status:?}: {message}")]
    DeviceError { status: u16, message: String },
    #[error("I/O error: {0}")]
    Io(#[from] std::io::Error),
    #[error("timeout waiting for response to seq {0}")]
    Timeout(u16),
    #[error("unsupported protocol flag 0x{0:02X}")]
    UnsupportedFlags(u8),
}

/// Decoded 16-byte Protocol v2 header.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct PacketHeader {
    pub version: u8,
    pub msg_type: u8,
    pub flags: u8,
    pub hdr_crc8: u8,
    pub seq: u16,
    pub status_or_sub: u16,
    pub payload_len: u32,
}

/// Complete decoded Protocol v2 packet.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Packet {
    pub header: PacketHeader,
    pub payload: Vec<u8>,
}

impl Packet {
    /// Construct and serialize a packet into its exact wire byte representation.
    pub fn encode(
        msg_type: MsgType,
        flags: u8,
        seq: u16,
        status_or_sub: u16,
        payload: &[u8],
    ) -> Result<Vec<u8>, ProtocolError> {
        if payload.len() > MAX_PAYLOAD_LEN {
            return Err(ProtocolError::PayloadTooLarge(payload.len()));
        }
        let mut header = [0u8; HEADER_LEN];
        header[0..4].copy_from_slice(&MAGIC);
        header[4] = PROTO_MAJOR;
        header[5] = msg_type as u8;
        header[6] = flags;
        header[8..10].copy_from_slice(&seq.to_le_bytes());
        header[10..12].copy_from_slice(&status_or_sub.to_le_bytes());
        header[12..16].copy_from_slice(&(payload.len() as u32).to_le_bytes());
        header[7] = crc::header_crc8(&header);

        let crc32 = crc::packet_crc32(&header, payload);
        let mut out = Vec::with_capacity(HEADER_LEN + payload.len() + TRAILER_LEN);
        out.extend_from_slice(&header);
        out.extend_from_slice(payload);
        out.extend_from_slice(&crc32.to_le_bytes());
        Ok(out)
    }

    /// Return the typed `MsgType`, if recognized.
    pub fn msg_type(&self) -> Result<MsgType, ProtocolError> {
        MsgType::try_from(self.header.msg_type)
    }

    /// True if the `FLAG_ERROR` bit is set.
    #[must_use]
    pub fn is_error(&self) -> bool {
        (self.header.flags & flags::ERROR) != 0
    }
}
