//! Bit-exact CRC-8/SMBUS and IEEE 802.3 CRC-32 checksums per `docs/protocol-v2.md` §2.4.

/// Precomputed 256-entry CRC-8/SMBUS (polynomial 0x07, init 0x00, no reflection) lookup table.
const fn build_crc8_table() -> [u8; 256] {
    let mut table = [0u8; 256];
    let mut i = 0usize;
    while i < 256 {
        let mut crc = i as u8;
        let mut bit = 0;
        while bit < 8 {
            if (crc & 0x80) != 0 {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
            bit += 1;
        }
        table[i] = crc;
        i += 1;
    }
    table
}

const CRC8_TABLE: [u8; 256] = build_crc8_table();

/// Compute CRC-8/SMBUS over `data`. Check value for `b"123456789"` is `0xF4`.
#[must_use]
pub fn crc8_smbus(data: &[u8]) -> u8 {
    let mut crc = 0u8;
    for &b in data {
        crc = CRC8_TABLE[(crc ^ b) as usize];
    }
    crc
}

/// Compute `hdr_crc8` over the 15 non-CRC header bytes (`header[0..7]` followed by `header[8..16]`).
#[must_use]
pub fn header_crc8(header: &[u8; 16]) -> u8 {
    let mut crc = 0u8;
    for &b in &header[0..7] {
        crc = CRC8_TABLE[(crc ^ b) as usize];
    }
    for &b in &header[8..16] {
        crc = CRC8_TABLE[(crc ^ b) as usize];
    }
    crc
}

/// Compute IEEE 802.3 CRC-32 over `data`. Check value for `b"123456789"` is `0xCBF43926`.
#[must_use]
pub fn crc32_ieee(data: &[u8]) -> u32 {
    crc32fast::hash(data)
}

/// Compute IEEE 802.3 CRC-32 over a 16-byte header followed immediately by `payload`.
#[must_use]
pub fn packet_crc32(header: &[u8; 16], payload: &[u8]) -> u32 {
    let mut hasher = crc32fast::Hasher::new();
    hasher.update(header);
    hasher.update(payload);
    hasher.finalize()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn golden_checksum_reference_vectors() {
        assert_eq!(crc8_smbus(b"123456789"), 0xF4);
        assert_eq!(crc32_ieee(b"123456789"), 0xCBF4_3926);
    }
}
