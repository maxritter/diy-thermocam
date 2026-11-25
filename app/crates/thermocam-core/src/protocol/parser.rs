//! Byte-oriented streaming parser with overlapping-magic 1-byte slide resync (§3.1).

use super::crc::{header_crc8, packet_crc32};
use super::{
    flags, Packet, PacketHeader, ProtocolError, HEADER_LEN, MAGIC, MAX_PAYLOAD_LEN, PROTO_MAJOR,
    TRAILER_LEN,
};

/// Parser diagnostics counters.
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct ParserStats {
    pub packets_ok: u64,
    pub usb_rx_errors: u64,
    pub usb_crc_errors: u64,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum State {
    FindMagic,
    ReadHeader,
    ReadPayloadAndCrc,
}

/// Incremental byte-oriented Protocol v2 parser.
///
/// Implements the exact overlapping-magic 1-byte slide resync specified in
/// `docs/protocol-v2.md` §3.1 so false `"TC2\0"` sequences inside corrupted
/// streams never eat a real packet whose magic begins inside bytes `1..15`.
#[derive(Debug)]
pub struct StreamParser {
    state: State,
    header_buf: [u8; HEADER_LEN],
    header_pos: usize,
    pending_header: Option<PacketHeader>,
    body_buf: Vec<u8>,
    body_needed: usize,
    stats: ParserStats,
}

impl Default for StreamParser {
    fn default() -> Self {
        Self::new()
    }
}

impl StreamParser {
    #[must_use]
    pub fn new() -> Self {
        Self {
            state: State::FindMagic,
            header_buf: [0u8; HEADER_LEN],
            header_pos: 0,
            pending_header: None,
            body_buf: Vec::new(),
            body_needed: 0,
            stats: ParserStats::default(),
        }
    }

    #[must_use]
    pub fn stats(&self) -> ParserStats {
        self.stats
    }

    pub fn reset(&mut self) {
        self.state = State::FindMagic;
        self.header_pos = 0;
        self.pending_header = None;
        self.body_buf.clear();
        self.body_needed = 0;
    }

    /// Feed an arbitrary slice of bytes (even 1 byte at a time) and collect all decoded packets.
    pub fn feed(&mut self, data: &[u8]) -> Vec<Result<Packet, ProtocolError>> {
        let mut results = Vec::new();
        for &byte in data {
            self.push_byte(byte, &mut results);
        }
        results
    }

    fn push_byte(&mut self, byte: u8, out: &mut Vec<Result<Packet, ProtocolError>>) {
        match self.state {
            State::FindMagic => {
                self.feed_magic_byte(byte);
            }
            State::ReadHeader => {
                self.header_buf[self.header_pos] = byte;
                self.header_pos += 1;
                if self.header_pos == HEADER_LEN {
                    self.try_accept_header(out);
                }
            }
            State::ReadPayloadAndCrc => {
                self.body_buf.push(byte);
                if self.body_buf.len() == self.body_needed {
                    let header = self
                        .pending_header
                        .take()
                        .expect("pending_header set in ReadPayloadAndCrc");
                    let payload_len = header.payload_len as usize;
                    let payload = &self.body_buf[..payload_len];
                    let wire_crc = u32::from_le_bytes([
                        self.body_buf[payload_len],
                        self.body_buf[payload_len + 1],
                        self.body_buf[payload_len + 2],
                        self.body_buf[payload_len + 3],
                    ]);
                    let calc_crc = packet_crc32(&self.header_buf, payload);
                    if wire_crc == calc_crc {
                        self.stats.packets_ok += 1;
                        if (header.flags & flags::COMPRESSED) != 0 {
                            out.push(Err(ProtocolError::UnsupportedFlags(header.flags)));
                        } else {
                            out.push(Ok(Packet {
                                header,
                                payload: payload.to_vec(),
                            }));
                        }
                    } else {
                        self.stats.usb_crc_errors += 1;
                        out.push(Err(ProtocolError::BadCrc {
                            expected: calc_crc,
                            actual: wire_crc,
                        }));
                    }
                    self.body_buf.clear();
                    self.body_needed = 0;
                    self.header_pos = 0;
                    self.state = State::FindMagic;
                }
            }
        }
    }

    fn feed_magic_byte(&mut self, byte: u8) {
        if byte == MAGIC[self.header_pos] {
            self.header_buf[self.header_pos] = byte;
            self.header_pos += 1;
            if self.header_pos == MAGIC.len() {
                self.state = State::ReadHeader;
            }
        } else if byte == MAGIC[0] {
            self.header_buf[0] = byte;
            self.header_pos = 1;
        } else {
            self.header_pos = 0;
        }
    }

    fn try_accept_header(&mut self, out: &mut Vec<Result<Packet, ProtocolError>>) {
        let version = self.header_buf[4];
        let msg_type = self.header_buf[5];
        let pkt_flags = self.header_buf[6];
        let hdr_crc = self.header_buf[7];
        let seq = u16::from_le_bytes([self.header_buf[8], self.header_buf[9]]);
        let status_or_sub = u16::from_le_bytes([self.header_buf[10], self.header_buf[11]]);
        let payload_len = u32::from_le_bytes([
            self.header_buf[12],
            self.header_buf[13],
            self.header_buf[14],
            self.header_buf[15],
        ]);

        let valid = version == PROTO_MAJOR
            && (payload_len as usize) <= MAX_PAYLOAD_LEN
            && header_crc8(&self.header_buf) == hdr_crc;

        if valid {
            self.pending_header = Some(PacketHeader {
                version,
                msg_type,
                flags: pkt_flags,
                hdr_crc8: hdr_crc,
                seq,
                status_or_sub,
                payload_len,
            });
            self.body_buf.clear();
            self.body_needed = payload_len as usize + TRAILER_LEN;
            self.body_buf.reserve(self.body_needed);
            self.state = State::ReadPayloadAndCrc;
            return;
        }

        // Invalid header: increment usb_rx_errors and slide header[1..16] left by 1 byte,
        // re-scanning the buffered 15 bytes before reading new bytes from the stream.
        self.stats.usb_rx_errors += 1;
        let tail: [u8; HEADER_LEN - 1] = self.header_buf[1..HEADER_LEN]
            .try_into()
            .expect("15-byte tail");
        self.header_pos = 0;
        self.state = State::FindMagic;
        for &b in &tail {
            self.push_byte(b, out);
        }
    }
}
