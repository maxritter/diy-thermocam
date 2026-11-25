# DIY-Thermocam USB Protocol v2.0 Specification

> **Status:** Authoritative wire specification for DIY-Thermocam firmware (`firmware/3.0/src/core/tc_proto_v2.*`, C++17) and the desktop application (`app/crates/thermocam-core`, Rust).
> **Protocol Version:** `2.0` (`proto_major = 2`, `proto_minor = 0`).
> **License:** GPL-3.0.

This specification defines the clean-slate binary USB communication protocol replacing the legacy v1 single-byte serial protocol. Every multi-byte integer and IEEE 754 floating-point field on the wire is **little-endian (LE)**, matching both the ARM Cortex-M7 (Teensy 4.1) and x86_64/arm64 hosts for zero-copy pixel transmission.

Legacy `.DAT` radiometric files on the SD card remain 100% unchanged and compatible across V1/V2/V3 cameras.

---

## 1. Transport & Session Lifecycle

### 1.1 Physical & USB Layer
- **Transport:** USB 2.0 High-Speed (480 Mbit/s) CDC ACM virtual serial port (`Serial` on Teensy 4.1, USB VID `0x16C0`, PJRC CDC PID `0x0483` for `usb=serial` or `0x04D5` for `usb=serialmtp` during transition).
- **Baud rate:** Ignored by USB CDC ACM data transfers (always runs at full USB HS bulk speed). Hosts SHOULD open the port at `115200` baud, `8N1`, no software flow control (`XON/XOFF` off), no parity, and assert **DTR = 1** (`RTS = 0` or `1`).
  - **CRITICAL:** Never open the port at **134 baud** during normal operation. Teensyduino's USB stack (`usb.c`) treats `CDC_SET_LINE_CODING == 134` as an automatic HalfKay bootloader reboot trigger (see §4.7).
- **Zero-copy writes:** Firmware writes packets via bulk `Serial.write(ptr, len)` + `Serial.send_now()` (never per-byte loops).

### 1.2 Session State Machine
1. **Idle / Standalone Live Mode:** Camera runs its normal touchscreen UI and Lepton acquisition loop while polling `Serial.available()` every main-loop iteration without blocking on frames.
2. **Handshake:** Host opens the port (asserting DTR), drains any stale OS buffer, and sends `HELLO_REQ (0x01)`.
   - Camera enters the active USB v2 session immediately (no 1-second blocking splash delay), stops turning the display into blocking menu loops, and replies with `HELLO_RESP (0x41)`.
   - By default, the LCD backlight turns off to save power unless `host_features & 0x01` (`KEEP_LCD_ON_DEFAULT`) or `STREAM_CTRL.keep_lcd_on == 1` is requested.
3. **Active Session:**
   - Full-duplex concurrency: Camera pushes `FRAME_RAW`/`FRAME_RENDERED` and `EVT_*` asynchronously while concurrently servicing any incoming request (`0x01..0x3F`, `0xE0..0xE5`, `0xF0`) between packets and Lepton segments (target command latency `< 2 ms`).
   - Host sends `PING_REQ (0x03)` periodically (recommended every `1000 ms`–`2000 ms`, or whenever idle). Any valid incoming host packet resets the camera's inactivity watchdog.
4. **Clean Exit & Watchdog Recovery:**
   - **Host-initiated:** Host sends `GOODBYE_REQ (0x02)` → camera stops streaming, sends `GOODBYE_RESP (0x42)`, flushes USB, restores standalone live mode and backlight.
   - **Device-initiated (touch):** Long touch on the camera screen (when touch is present) sends `EVT_SESSION_EXIT (0xC4, reason = 0)` and returns to standalone live mode.
   - **Disconnect / Timeout:** If USB DTR drops (`!Serial`) **or** no valid host packet is received for `heartbeat_timeout_ms` (default **`5000 ms`**), the camera stops streaming, emits `EVT_SESSION_EXIT (0xC4, reason = 1 or 3)` if USB is still writable, and returns cleanly to standalone live mode.

### 1.3 Phase 3 Transition Note (Auto-Detect → Clean Cutover)
During Phase 3 implementation only, `checkSerial()` / `serialInit()` temporarily distinguish sessions on the first byte received while idle:
- `0x64` (`CMD_START`) → legacy v1 `serialConnect()` (keeps the original 34-test v1 regression suite runnable while porting).
- `0x54` (`'T'`) followed within `50 ms` by `0x43 0x32 0x00` (`"TC2\0"`) → v2 session (note `0x54` collides with no v1 command byte).

Once the Tauri v2 desktop app and the ported v2 hardware regression suite pass on real hardware, the temporary `0x64` v1 branch is **deleted** (clean v2-only cutover for firmware 3.1+). Legacy v3.0.x cameras remain upgradeable from the desktop app via the 134-baud CDC bootloader trigger (§4.7).

---

## 2. Packet Framing & Checksums

Every message in both directions uses the same binary frame layout: **16-byte header + variable payload + 4-byte CRC-32 trailer** (20 bytes total overhead).

```text
+----------------------------------+-------------------------+----------------+
|         Header (16 B)            |   Payload (0..163840 B) |  CRC-32 (4 B)  |
+----------------------------------+-------------------------+----------------+
|<-------------------- Covered by trailing CRC-32 ---------->|
```

### 2.1 Header Format (16 Bytes, Little-Endian)

| Offset | Size | Type | Field | Description |
|---:|---:|---|---|---|
| `0` | 4 | `u8[4]` | `magic` | Fixed sync sequence: `0x54, 0x43, 0x32, 0x00` (ASCII `"TC2\0"`). |
| `4` | 1 | `u8` | `version` | Protocol major version. MUST be `0x02`. |
| `5` | 1 | `u8` | `msg_type` | Message type ID (§4). |
| `6` | 1 | `u8` | `flags` | Bitmask of packet flags (§2.2). |
| `7` | 1 | `u8` | `hdr_crc8` | **CRC-8/SMBUS** over the other 15 header bytes (`bytes[0..6]` followed by `bytes[8..15]`). |
| `8` | 2 | `u16` | `seq` | Sequence number (LE). Host increments per request; camera echoes exact `seq` in response(s). Camera uses an independent monotonic counter for unsolicited streams (`0x80..0x81`) and async events (`0xC0..0xC5`). |
| `10` | 2 | `u16` | `status_or_sub` | In response packets (`0x40..0x7F`, `0xE8..0xED`, `0xF1`, `0xFF`): `StatusCode` (§3.2; `0x0000 = OK`). In requests, streams, and events: sub-channel/subtype (must be `0` unless specified). |
| `12` | 4 | `u32` | `payload_len` | Payload byte length. MUST satisfy `0 <= payload_len <= 163840` (`MAX_PAYLOAD_LEN`). |

### 2.2 Packet Flags (`flags: u8`)

| Bit | Mask | Constant | Semantics |
|---:|---|---|---|
| 0 | `0x01` | `FLAG_ACK_REQ` | Sender requests explicit response/acknowledgement (informational on requests; `0` on responses/streams). |
| 1 | `0x02` | `FLAG_ERROR` | MUST be set on any response with `status_or_sub != 0x0000`; MUST be `0` when `status_or_sub == 0x0000`. When set, payload is empty or an optional UTF-8 diagnostic string (`<= 128 B`). |
| 2 | `0x04` | `FLAG_MORE_CHUNKS` | Set on multi-packet responses (`FS_READ_DATA`) when additional packets with the same `seq` follow. Cleared (`0`) on the final packet. |
| 3 | `0x08` | `FLAG_COMPRESSED` | Reserved for future payload compression. MUST be `0` in v2.0 (receivers MUST reject with `ERR_UNSUPPORTED` if set). |
| 4..7 | `0xF0` | Reserved | MUST be `0` on send; ignored on receive in v2.0. |

### 2.3 Trailer (4 Bytes, Little-Endian)

| Offset | Size | Type | Field | Description |
|---:|---:|---|---|---|
| `16 + payload_len` | 4 | `u32` | `crc32` | **IEEE 802.3 CRC-32** over all `16 + payload_len` bytes: `header[0..16]` (including `hdr_crc8` at byte 7) concatenated with `payload[0..payload_len]`. |

### 2.4 Checksum Specifications (Bit-Exact)

1. **`hdr_crc8` — CRC-8/SMBUS (CRC-8/ATM)**:
   - Polynomial: `0x07` ($x^8 + x^2 + x + 1$)
   - Initial value: `0x00`
   - Reflect input (`refin`): `false`; Reflect output (`refout`): `false`; Final XOR (`xorout`): `0x00`
   - Check value for ASCII `"123456789"`: **`0xF4`**
   - Exact input byte sequence (15 bytes): `header[0], header[1], header[2], header[3], header[4], header[5], header[6], header[8], header[9], header[10], header[11], header[12], header[13], header[14], header[15]`.
   - Purpose: Allows immediate rejection of false `"TC2\0"` occurrences inside raw pixel streams during mid-stream resync without stalling for a bogus `payload_len`.

2. **`crc32` — Standard IEEE 802.3 CRC-32**:
   - Normal polynomial: `0x04C11DB7` (reflected: `0xEDB88320`)
   - Initial value: `0xFFFFFFFF`
   - Reflect input (`refin`): `true`; Reflect output (`refout`): `true`; Final XOR (`xorout`): `0xFFFFFFFF`
   - Check value for ASCII `"123456789"`: **`0xCBF43926`**
   - Matches Python `zlib.crc32(data) & 0xFFFFFFFF` and Rust `crc32fast::hash(data)`.
   - Exact input byte sequence (`16 + payload_len` bytes): `header[0..16]` followed immediately by `payload[0..payload_len]`.

---

## 3. Streaming Parser, Resync, Errors & Backpressure

### 3.1 Streaming Parser State Machine
Both firmware (`tc_proto_v2`) and host (`thermocam-core`) MUST implement a byte-oriented incremental parser that handles arbitrary USB packet splits and coalescing:

```text
  +------------+   matched "TC2\0"    +-------------+  valid header   +----------------------+
  | FIND_MAGIC | -------------------> | READ_HEADER | --------------> | READ_PAYLOAD_AND_CRC |
  +------------+                      +-------------+                 +----------------------+
        ^                                    |                                   |
        |   invalid version / payload_len    |                                   |
        |   or hdr_crc8 mismatch:            |                                   | CRC-32 OK -> dispatch
        +--- SLIDE header[1..16] left 1 B ---+                                   | CRC-32 FAIL -> inc crc_errors,
        ^                                                                        |   reply ERR_BAD_CRC (if req),
        +------------------------------------------------------------------------+   return to FIND_MAGIC
```

**Critical resync rule (overlapping magic):**
When `READ_HEADER` completes 16 bytes and fails any of:
- `header[4] != 0x02` (`version`)
- `payload_len > 163840` (`MAX_PAYLOAD_LEN`)
- `crc8_smbus(header[0..7] || header[8..16]) != header[7]` (`hdr_crc8`)

the parser MUST increment `usb_rx_errors`, **shift `header[1..16]` left by 1 byte**, and scan those remaining 15 buffered bytes for `"TC2\0"` before consuming new bytes from the serial port. Discarding all 16 bytes on a false sync would drop a real packet whose magic started inside bytes `1..15`.

**Receive timeouts:**
- **Firmware RX timeout:** Once a valid header is accepted in `READ_PAYLOAD_AND_CRC`, if no new byte arrives for `500 ms` before `payload_len + 4` bytes complete, abort the partial packet, increment `usb_rx_errors`, and return to `FIND_MAGIC`.
- **Host request timeout:** Default `1000 ms` for control commands (`3000 ms` for `TRIGGER_FFC`, `SAVE_CAPTURE`, and `FS_READ` windows).

### 3.2 Error Status Codes (`status_or_sub: u16`)

| Code | Name | Meaning |
|---:|---|---|
| `0x0000` | `OK` | Request succeeded (`FLAG_ERROR` clear). |
| `0x0001` | `ERR_BAD_CRC` | Payload CRC-32 failed on a request whose header passed `hdr_crc8` (camera echoes `seq` via `GENERIC_ERROR_RESP`). |
| `0x0002` | `ERR_UNKNOWN_MSG` | Unrecognized `msg_type` (camera replies with `GENERIC_ERROR_RESP`). |
| `0x0003` | `ERR_INVALID_ARG` | Wrong payload length, parameter out of range, or malformed UTF-8/path. Setting commands MUST validate atomically and change no state on error. |
| `0x0004` | `ERR_BUSY` | Subsystem busy (e.g., FFC already executing, concurrent `FS_READ` active). |
| `0x0005` | `ERR_IO` | Hardware I/O failure (Lepton I2C/SPI timeout, SD read/write failure). |
| `0x0006` | `ERR_NOT_FOUND` | Requested SD file or directory does not exist. |
| `0x0007` | `ERR_FORBIDDEN` | Security/policy refusal (path containing `..`, backslash `\`, deleting `"/"`, FFC in manual mode without force flag). |
| `0x0008` | `ERR_UNSUPPORTED` | Feature unavailable on this device (e.g., `CAP_LEPTON_RAD_PARAMS` or `CAP_DISPLAY_STREAM` not supported, `FLAG_COMPRESSED` set). |
| `0x0009` | `ERR_BAD_MAGIC` | Safety magic mismatch (e.g., `ENTER_BOOTLOADER_REQ` magic != `0xB00710AD`). |
| `0x000A` | `ERR_NO_SD` | SD card absent, unmounted, or insufficient free space (`< 1 MB` for capture). |

### 3.3 Backpressure & Frame Drop Policy
- **Control responses & events have priority over stream frames.**
- Before writing a `FRAME_RAW` or `FRAME_RENDERED` packet, firmware checks `Serial.availableForWrite()` and enforces a bounded TX write budget (`<= 15 ms` per frame).
- If USB TX buffers are stalled (host paused or disconnected), firmware **drops the frame before starting transmission** (never truncates mid-packet), increments `dropped_frames`, and continues servicing the main loop and incoming control requests.
- If a write stalls mid-packet for `> 100 ms`, firmware aborts the stream and relies on the 5 s heartbeat / DTR watchdog to tear down the session cleanly.

---

## 4. Complete Message Catalog

### 4.1 Message Type ID Allocation

Convention: Control requests `0x01..0x3F` map to responses `req | 0x40` (`0x41..0x7F`); SD requests `0xE0..0xE5` map to responses `req | 0x08` (`0xE8..0xED`).

| Range | Req ID | Request Name | Resp ID | Response / Push Name | Direction |
|---|---:|---|---:|---|---|
| Lifecycle | `0x01` | `HELLO_REQ` | `0x41` | `HELLO_RESP` | Host ↔ Cam |
| Lifecycle | `0x02` | `GOODBYE_REQ` | `0x42` | `GOODBYE_RESP` | Host ↔ Cam |
| Lifecycle | `0x03` | `PING_REQ` | `0x43` | `PONG_RESP` | Host ↔ Cam |
| Streaming | `0x10` | `STREAM_CTRL_REQ` | `0x50` | `STREAM_CTRL_RESP` | Host ↔ Cam |
| Config/Ctrl | `0x11` | `GET_CONFIG_REQ` | `0x51` | `GET_CONFIG_RESP` | Host ↔ Cam |
| Config/Ctrl | `0x12` | `SET_CONFIG_REQ` | `0x52` | `SET_CONFIG_RESP` | Host ↔ Cam |
| Config/Ctrl | `0x13` | `SET_TIME_REQ` | `0x53` | `SET_TIME_RESP` | Host ↔ Cam |
| Config/Ctrl | `0x14` | `TRIGGER_FFC_REQ` | `0x54` | `TRIGGER_FFC_RESP` | Host ↔ Cam |
| Config/Ctrl | `0x15` | `GET_TEMP_POINTS_REQ` | `0x55` | `GET_TEMP_POINTS_RESP` | Host ↔ Cam |
| Config/Ctrl | `0x16` | `SET_TEMP_POINTS_REQ` | `0x56` | `SET_TEMP_POINTS_RESP` | Host ↔ Cam |
| Config/Ctrl | `0x17` | `SAVE_CAPTURE_REQ` | `0x57` | `SAVE_CAPTURE_RESP` | Host ↔ Cam |
| Config/Ctrl | `0x18` | `GET_DIAGNOSTICS_REQ` | `0x58` | `GET_DIAGNOSTICS_RESP` | Host ↔ Cam |
| Config/Ctrl | `0x19` | `GET_PERF_REQ` | `0x59` | `GET_PERF_RESP` | Host ↔ Cam |
| Radiometry | `0x1A` | `GET_RAD_PARAMS_REQ` | `0x5A` | `GET_RAD_PARAMS_RESP` | Host ↔ Cam |
| Radiometry | `0x1B` | `SET_RAD_PARAMS_REQ` | `0x5B` | `SET_RAD_PARAMS_RESP` | Host ↔ Cam |
| Push Stream | — | — | `0x80` | `FRAME_RAW` | Cam → Host |
| Push Stream | — | — | `0x81` | `FRAME_RENDERED` | Cam → Host |
| Async Event | — | — | `0xC0` | `EVT_BUTTON` | Cam → Host |
| Async Event | — | — | `0xC1` | `EVT_TOUCH` | Cam → Host |
| Async Event | — | — | `0xC2` | `EVT_BATTERY` | Cam → Host |
| Async Event | — | — | `0xC3` | `EVT_FFC` | Cam → Host |
| Async Event | — | — | `0xC4` | `EVT_SESSION_EXIT` | Cam → Host |
| Async Event | — | — | `0xC5` | `EVT_PERF` | Cam → Host |
| SD FS | `0xE0` | `FS_INFO_REQ` | `0xE8` | `FS_INFO_RESP` | Host ↔ Cam |
| SD FS | `0xE1` | `FS_LIST_REQ` | `0xE9` | `FS_LIST_RESP` | Host ↔ Cam |
| SD FS | `0xE2` | `FS_STAT_REQ` | `0xEA` | `FS_STAT_RESP` | Host ↔ Cam |
| SD FS | `0xE3` | `FS_READ_REQ` | `0xEB` | `FS_READ_DATA` (chunked) | Host ↔ Cam |
| SD FS | `0xE4` | `FS_DELETE_REQ` | `0xEC` | `FS_DELETE_RESP` | Host ↔ Cam |
| SD FS | `0xE5` | `FS_ABORT_REQ` | `0xED` | `FS_ABORT_RESP` | Host ↔ Cam |
| System | `0xF0` | `ENTER_BOOTLOADER_REQ` | `0xF1` | `ENTER_BOOTLOADER_RESP` | Host ↔ Cam |
| Error | — | — | `0xFF` | `GENERIC_ERROR_RESP` | Cam → Host |

---

### 4.2 Lifecycle (`0x01–0x03`, `0x41–0x43`)

#### `HELLO_REQ (0x01)` — 8 bytes

| Offset | Size | Type | Field | Description |
|---:|---:|---|---|---|
| `0` | 1 | `u8` | `host_proto_major` | Host protocol major (`2`). |
| `1` | 1 | `u8` | `host_proto_minor` | Host protocol minor (`0`). |
| `2` | 2 | `u16` | `reserved` | Must be `0`. |
| `4` | 4 | `u32` | `host_features` | Bitmask: `bit 0` = `KEEP_LCD_ON_DEFAULT`, `bit 1` = `SUPPORTS_PERF_EVENTS`. |

#### `HELLO_RESP (0x41)` — 64 bytes

| Offset | Size | Type | Field | Description |
|---:|---:|---|---|---|
| `0` | 1 | `u8` | `proto_major` | Device protocol major (`2`). |
| `1` | 1 | `u8` | `proto_minor` | Device protocol minor (`0`). |
| `2` | 2 | `u16` | `fw_version` | Integer firmware version (e.g., `310` for `3.1.0`). |
| `4` | 32 | `u8[32]` | `fw_build_str` | Null-padded ASCII/UTF-8 build string (e.g., `"3.1.0 (2026-11-25)"`). |
| `36` | 1 | `u8` | `hw_version` | Hardware revision (`3` = DIY-Thermocam V3, `2` = V2). |
| `37` | 1 | `u8` | `lepton_type` | `0` = Lepton 2.5 (80×60), `1` = Lepton 3.5 / 3.1R (160×120), `255` = absent/unknown. |
| `38` | 2 | `u16` | `sensor_w` | Native sensor width (`80` or `160`). |
| `40` | 2 | `u16` | `sensor_h` | Native sensor height (`60` or `120`). |
| `42` | 2 | `u16` | `display_w` | On-device LCD width (`320`, or `0` if headless). |
| `44` | 2 | `u16` | `display_h` | On-device LCD height (`240`, or `0` if headless). |
| `46` | 1 | `u8` | `diagnostic_mask` | Bitmask (`1 = OK`, `0 = fault`): `bit 0` display, `bit 1` touch, `bit 2` SD, `bit 3` battery, `bit 4` Lepton, bits `5..7` = `1`. |
| `47` | 1 | `u8` | `reserved` | `0`. |
| `48` | 8 | `u64` | `teensy_uid` | 64-bit unique Teensy 4.1 hardware ID (`OCOTP_CFG0/1` or MAC) for multi-camera identification. |
| `56` | 4 | `u32` | `capabilities` | Capability bitmask (table below). |
| `60` | 2 | `u16` | `max_chunk_size` | Maximum `FS_READ_DATA` chunk payload bytes (default `8192`). |
| `62` | 2 | `u16` | `heartbeat_timeout_ms` | Session inactivity timeout in ms (default `5000`). |

**`capabilities` bitmask (`u32`):**

| Bit | Mask | Constant | Meaning |
|---:|---|---|---|
| 0 | `0x0001` | `CAP_RAW_STREAM` | Supports `FRAME_RAW (0x80)` push streaming. |
| 1 | `0x0002` | `CAP_DISPLAY_STREAM` | Supports `FRAME_RENDERED (0x81)` 320×240 RGB565 stream. |
| 2 | `0x0004` | `CAP_SD_FS` | Supports SD filesystem commands (`0xE0–0xE5`). |
| 3 | `0x0008` | `CAP_PERF_TELEMETRY` | Supports `GET_PERF (0x19)` and `EVT_PERF (0xC5)`. |
| 4 | `0x0010` | `CAP_BOOTLOADER` | Supports `ENTER_BOOTLOADER (0xF0)`. |
| 5 | `0x0020` | `CAP_LEPTON_RAD_PARAMS` | Supports `GET_RAD_PARAMS (0x1A)` / `SET_RAD_PARAMS (0x1B)` via Lepton CCI `0x4EBC`/`0x4EBD`. |
| 6 | `0x0040` | `CAP_GAIN_MODE` | Supports high/low Lepton gain switching (`DeviceConfig.lepton_gain_mode`). |
| 7 | `0x0080` | `CAP_HOT_COLD` | Supports hot/cold threshold highlighting in `DeviceConfig`. |
| 8 | `0x0100` | `CAP_RTC_SYNC` | Supports `SET_TIME (0x13)`. |
| 9 | `0x0200` | `CAP_KEEP_LCD_ON` | Supports keeping the on-device LCD live while streaming over USB. |

#### `GOODBYE_REQ (0x02)` / `GOODBYE_RESP (0x42)` — 0 bytes
Clean session termination. Camera stops streaming, sends `GOODBYE_RESP (OK)`, flushes USB, and returns to standalone live mode.

#### `PING_REQ (0x03)` — 8 bytes / `PONG_RESP (0x43)` — 16 bytes
- `PING_REQ`: `0..7: host_time_us: u64` (host monotonic µs for RTT measurement).
- `PONG_RESP`: `0..7: host_time_us: u64` (echoed verbatim), `8..15: device_uptime_us: u64`.

---

### 4.3 Streaming (`0x10`, `0x50`, `0x80–0x81`)

#### `STREAM_CTRL_REQ (0x10)` / `STREAM_CTRL_RESP (0x50)` — 8 bytes

| Offset | Size | Type | Field | Description |
|---:|---:|---|---|---|
| `0` | 1 | `u8` | `mode` | `0` = OFF, `1` = `FRAME_RAW`, `2` = `FRAME_RENDERED`, `3` = `RAW_AND_RENDERED`. |
| `1` | 1 | `u8` | `keep_lcd_on` | `0` = backlight off, `1` = keep LCD rendering live while streaming. |
| `2` | 1 | `u8` | `include_temp_points` | `0` = omit, `1` = append active sparse temp points to `FRAME_RAW`. |
| `3` | 1 | `u8` | `perf_interval_s` | `0` = off, `1..60` = push `EVT_PERF (0xC5)` every `N` seconds. |
| `4` | 2 | `u16` | `max_fps_x10` | `0` = native Lepton rate (~8.78 Hz); otherwise max rate in tenths of Hz (e.g. `50` = 5.0 Hz). |
| `6` | 2 | `u16` | `reserved` | Must be `0`. |

`STREAM_CTRL_RESP` returns the active (clamped) 8-byte settings. Requesting `mode == 2` or `3` without `CAP_DISPLAY_STREAM` returns `ERR_UNSUPPORTED`.

#### `FRAME_RAW (0x80)` — `64 + (width * height * 2) + (temp_point_count * 6)` bytes

**Fixed 64-byte LE Metadata Header:**

| Offset | Size | Type | Field | Description |
|---:|---:|---|---|---|
| `0` | 4 | `u32` | `frame_seq` | Monotonic acquired-frame counter since boot. |
| `4` | 8 | `u64` | `timestamp_us` | 64-bit monotonic Teensy uptime (µs) at frame completion. |
| `12` | 4 | `u32` | `rtc_epoch` | Unix timestamp (seconds) from Teensy RTC. |
| `16` | 4 | `u32` | `dropped_frames` | Cumulative frames dropped since `STREAM_CTRL` enabled streaming. |
| `20` | 2 | `u16` | `width` | Frame width in pixels (`160` for Lepton 3.5/3.1R, `80` for Lepton 2.5). |
| `22` | 2 | `u16` | `height` | Frame height in pixels (`120` or `60`). |
| `24` | 1 | `u8` | `pixel_format` | `0` = `UINT16_RAW_LE` (Lepton T-Linear radiometric counts). |
| `25` | 1 | `u8` | `gain_mode` | `0` = High gain (`-10..+140 °C`, `0.01 K/cnt`), `1` = Low gain (`-10..+450 °C`, `0.1 K/cnt`). |
| `26` | 1 | `u8` | `ffc_state` | `0` = idle, `1` = FFC in progress, `2` = FFC completed since previous frame. |
| `27` | 1 | `u8` | `flags` | `bit 0`: `limits_locked`, `bit 1`: `auto_mode`, `bit 2`: `temp_points_present`, `bit 3`: `rotation_vert`, `bit 4`: `rotation_horiz`. |
| `28` | 2 | `u16` | `min_raw` | Minimum raw count in frame (or active locked/manual lower bound). |
| `30` | 2 | `u16` | `max_raw` | Maximum raw count in frame (or active locked/manual upper bound). |
| `32` | 2 | `u16` | `min_x` | X coordinate of coldest pixel (`0..width-1`). |
| `34` | 2 | `u16` | `min_y` | Y coordinate of coldest pixel (`0..height-1`). |
| `36` | 2 | `u16` | `max_x` | X coordinate of hottest pixel (`0..width-1`). |
| `38` | 2 | `u16` | `max_y` | Y coordinate of hottest pixel (`0..height-1`). |
| `40` | 4 | `f32` | `spot_temp_c` | Lepton center spotmeter temperature in **°C** (always Celsius on wire). |
| `44` | 4 | `f32` | `cal_offset` | Linear temperature offset in °C (`-273.15f`). |
| `48` | 4 | `f32` | `cal_slope` | Linear temperature slope in K/count (`0.01f` high gain, `0.1f` low gain). `T_C = raw * cal_slope + cal_offset`. |
| `52` | 4 | `f32` | `fpa_temp_c` | Lepton FPA/housing temperature in °C, or `NaN` (`0x7FC00000`) if not polled. |
| `56` | 1 | `i8` | `bat_pct` | Battery percentage (`0..100`), or `-1` (`0xFF`) when running on USB power only. |
| `57` | 1 | `u8` | `usb_power` | `0` = battery only, `1` = USB VBUS detected. |
| `58` | 2 | `u16` | `temp_point_count` | Number of trailing sparse temp points (`0` when `flags & 0x04 == 0`, else `0..96`). |
| `60` | 4 | `u8[4]` | `reserved` | `0`. |

- **Pixels (offset `64`, size `width * height * 2` bytes):** Row-major `u16` LE radiometric values (`38,400 B` for 160×120; `9,600 B` for 80×60).
- **Trailing Sparse Temp Points (offset `64 + width * height * 2`, present iff `flags & 0x04`):** `temp_point_count` records of **6 bytes** each: `{ x: u16, y: u16, raw: u16 }`.

#### `FRAME_RENDERED (0x81)` — `16 + (width * height * 2)` bytes
Optional on-device rendered screen mirror (used for diagnostics, screen capture, and regression tests):
- `0..3`: `frame_seq: u32`
- `4..11`: `timestamp_us: u64`
- `12..13`: `width: u16` (`320`)
- `14..15`: `height: u16` (`240`)
- `16..153615`: `width * height` pixels of `u16` LE **RGB565** (`R[15:11] | G[10:5] | B[4:0]`). Total payload = `153,616 B`.

---

### 4.4 Configuration & Control (`0x11–0x1B`, `0x51–0x5B`)

#### `DeviceConfig` (32 bytes, used by `GET_CONFIG_RESP (0x51)`, `SET_CONFIG_REQ (0x12)`, `SET_CONFIG_RESP (0x52)`)

| Offset | Size | Type | Field | Valid Values |
|---:|---:|---|---|---|
| `0` | 1 | `u8` | `config_ver` | `1` (reject other values with `ERR_INVALID_ARG`). |
| `1` | 1 | `u8` | `color_scheme` | `0..18` (`colorScheme_arctic` .. `colorScheme_yellow`). |
| `2` | 1 | `u8` | `text_color` | `0..4` (`0` white, `1` black, `2` red, `3` green, `4` blue). |
| `3` | 1 | `u8` | `temp_format` | `0` = Celsius, `1` = Fahrenheit (affects on-device LCD & `.DAT` metadata). |
| `4` | 1 | `u8` | `filter_type` | `0` = none, `1` = gaussian, `2` = box. |
| `5` | 1 | `u8` | `min_max_points` | `0` = off, `1` = min, `2` = max, `3` = both. |
| `6` | 1 | `u8` | `rotation_vert` | `0` = normal, `1` = 180° rotated. |
| `7` | 1 | `u8` | `rotation_horiz` | `0` = normal, `1` = horizontally mirrored. |
| `8` | 1 | `u8` | `limits_mode` | `0` = auto, `1` = locked, `2` = manual (`manual_min_raw`/`manual_max_raw`). |
| `9` | 1 | `u8` | `shutter_mode` | `0` = manual FFC, `1` = automatic FFC. |
| `10` | 1 | `u8` | `lepton_gain_mode` | `0` = high gain (`-10..+140 °C`), `1` = low gain (`-10..+450 °C`). |
| `11` | 1 | `u8` | `hot_cold_mode` | `0` = disabled, `1` = cold, `2` = hot. |
| `12` | 1 | `u8` | `hot_cold_color` | `0..4` (white/black/red/green/blue). |
| `13` | 1 | `u8` | `screen_off_time` | `0` = disabled, `1` = 5 min, `2` = 20 min. |
| `14` | 2 | `u16` | `overlay_flags` | `bit 0` spot, `bit 1` colorbar, `bit 2` battery, `bit 3` time, `bit 4` date, `bit 5` storage, `bit 6` convert_bmp. |
| `16` | 2 | `u16` | `manual_min_raw` | Manual lower raw bound (must be `< manual_max_raw` when `limits_mode == 2`). |
| `18` | 2 | `u16` | `manual_max_raw` | Manual upper raw bound. |
| `20` | 2 | `i16` | `hot_cold_level_c10` | Hot/cold threshold in tenths of °C (e.g. `350` = `35.0 °C`). |
| `22` | 1 | `u8` | `min_max_preset` | `0` = temporary, `1..3` = EEPROM preset 1..3. |
| `23` | 9 | `u8[9]` | `reserved` | Must be `0`. |

- **`GET_CONFIG_REQ (0x11)`** (0 B) → **`GET_CONFIG_RESP (0x51)`** (32 B `DeviceConfig`).
- **`SET_CONFIG_REQ (0x12)`** (36 B): `0: persist: u8` (`0` = RAM-only, `1` = write changed values to EEPROM), `1..3: reserved[3]`, `4..35: DeviceConfig` → **`SET_CONFIG_RESP (0x52)`** (32 B active `DeviceConfig`). Validation is **atomic**: any invalid field returns `ERR_INVALID_ARG` without mutating state.

#### `SET_TIME_REQ (0x13)` / `SET_TIME_RESP (0x53)` — 4 bytes
- `0..3: rtc_epoch: u32` (Unix seconds written to `Teensy3Clock` / `setTime`, echoed back in response).

#### `TRIGGER_FFC_REQ (0x14)` — 1 byte / `TRIGGER_FFC_RESP (0x54)` — 2 bytes
- Request: `0: flags: u8` (`bit 0 = force_in_manual`).
- Response: `0: ffc_status: u8` (`0 = started/completed`), `1: reserved: u8`. Camera emits `EVT_FFC (started)` and `EVT_FFC (completed/error)`. Returns `ERR_FORBIDDEN` if in manual limits mode without `bit 0`, or `ERR_BUSY` if FFC is already active.

#### `GET_TEMP_POINTS_REQ (0x15)` / `GET_TEMP_POINTS_RESP (0x55)`
- Request: 0 bytes.
- Response: `0..1: count: u16` (`0..96`), followed by `count` × 6-byte entries `{ x: u16, y: u16, raw: u16 }` in sensor coordinates.

#### `SET_TEMP_POINTS_REQ (0x16)` / `SET_TEMP_POINTS_RESP (0x56)`
- Request: `0..1: count: u16` (`0..96`; `0` clears all points), followed by `count` × 4-byte entries `{ x: u16, y: u16 }` (`x < sensor_w`, `y < sensor_h`).
- Response: `0..1: count: u16` (active point count).

#### `SAVE_CAPTURE_REQ (0x17)` — 4 bytes / `SAVE_CAPTURE_RESP (0x57)` — 40 bytes
- Request: `0: flags: u8` (`bit 0 = save_bmp`, `bit 1 = override_convert_setting`, `bit 2 = wait_fresh_frame`), `1..3: reserved[3]`.
- Response: `0..3: dat_size: u32`, `4..7: bmp_size: u32` (`0` if omitted), `8..39: stem: u8[32]` (null-padded stem without extension, e.g., `"20261125103045"`). Returns `ERR_NO_SD` if card is missing or `< 1 MB` free.

#### `GET_DIAGNOSTICS_REQ (0x18)` (0 B) / `GET_DIAGNOSTICS_RESP (0x58)` — 80 bytes

| Offset | Size | Type | Field | Description |
|---:|---:|---|---|---|
| `0` | 1 | `u8` | `diagnostic_mask` | Same 5-bit mask as `HELLO_RESP` (`1 = OK`, `0 = fault`). |
| `1` | 1 | `i8` | `bat_pct` | Battery `%` (`0..100`) or `-1` (USB power only). |
| `2` | 1 | `u8` | `usb_power` | `0` = battery, `1` = USB VBUS connected. |
| `3` | 1 | `u8` | `sd_fs_type` | `0` = none, `1` = FAT16, `2` = FAT32, `3` = exFAT. |
| `4` | 2 | `u16` | `bat_mv` | Measured LiPo voltage in millivolts (`0` if USB-only). |
| `6` | 2 | `u16` | `cpu_mhz` | Actual ARM clock (`F_CPU_ACTUAL / 1_000_000`, e.g., `450`). |
| `8` | 4 | `f32` | `mcu_temp_c` | Teensy 4.1 internal die temperature (`tempmonGetTemp()`, °C). |
| `12` | 4 | `u32` | `sd_total_kb` | SD card capacity in KiB (`0` if absent). |
| `16` | 4 | `u32` | `sd_free_kb` | SD card free space in KiB (`0` if absent). |
| `20` | 8 | `u64` | `uptime_us` | Monotonic Teensy uptime (µs). |
| `28` | 4 | `u32` | `free_dtcm_bytes` | Available DTCM (RAM1) bytes. |
| `32` | 4 | `u32` | `free_ocram_bytes` | Available OCRAM/DMAMEM (RAM2) bytes. |
| `36` | 4 | `f32` | `lepton_fpa_temp_c` | Lepton FPA temperature (°C) or `NaN`. |
| `40` | 32 | `u8[32]` | `lepton_part_num` | Null-padded Lepton OEM part string (CCI `0x481C`, e.g. `"05-070170-00"`). |
| `72` | 8 | `u8[8]` | `reserved` | `0`. |

#### `GET_PERF_REQ (0x19)` — 1 byte / `GET_PERF_RESP (0x59)` — 84 bytes (`PerfTelemetry`)
- Request: `0: flags: u8`:
  - `bit 0 (0x01)` `reset_counters`: reset cumulative counters (`frames_dropped`, `vospi_resyncs`, `vospi_crc_errors`, `usb_rx_errors`, `usb_crc_errors`) after capturing the snapshot.
  - `bit 1 (0x02)` `select_last_video`: populate per-stage timings (`0..39`) and `frame_period_us` from the retained snapshot of the most recent on-device SD video recording (with `perf_mode = 1`), rather than the current live/serial session.
- Response (84 bytes, identical to `EVT_PERF (0xC5)` payload). `live_fps_x100` and `video_fps_x100` are retained in RAM across USB connect so hosts can benchmark standalone live mode and on-device video recording empirically after a run:

| Offset | Size | Type | Field | Description |
|---:|---:|---|---|---|
| `0` | 4 | `u32` | `acquire_us` | Last frame VSYNC → double-buffer ready duration (µs). |
| `4` | 4 | `u32` | `stats_us` | Spotmeter I2C + min/max + temp points duration (µs). |
| `8` | 4 | `u32` | `filter_us` | Box/gaussian filter duration (µs). |
| `12` | 4 | `u32` | `scale_us` | `smallToBigBuffer` upscaling duration (µs). |
| `16` | 4 | `u32` | `colorize_us` | `convertColors` LUT duration (µs). |
| `20` | 4 | `u32` | `overlay_us` | `displayInfos` overlay rendering duration (µs). |
| `24` | 4 | `u32` | `display_us` | LCD SPI/DMA flush duration (µs). |
| `28` | 4 | `u32` | `sd_write_us` | Most recent SD capture / video-frame write duration (µs). |
| `32` | 4 | `u32` | `usb_tx_us` | Most recent USB frame transmit duration (µs). |
| `36` | 4 | `u32` | `loop_total_us` | Total main-loop frame cycle duration (µs). |
| `40` | 4 | `u32` | `frame_count` | Total Lepton frames acquired since boot (or counter reset). |
| `44` | 4 | `u32` | `frames_dropped` | Total frames dropped (USB backpressure, SD stall, or missed VSYNC). |
| `48` | 4 | `u32` | `vospi_resyncs` | Total Lepton VoSPI segment/sync resync events (SPI signal-integrity monitor). |
| `52` | 4 | `u32` | `vospi_crc_errors` | Total Lepton VoSPI packet CRC-16/CCITT errors detected on `SPI1`. |
| `56` | 4 | `u32` | `usb_rx_errors` | Total USB header resyncs / malformed packets / RX timeouts. |
| `60` | 4 | `u32` | `usb_crc_errors` | Total USB packets rejected for CRC-32 mismatch. |
| `64` | 2 | `u16` | `live_fps_x100` | Rolling on-device standalone live-mode FPS in centi-Hz (e.g. `878` = `8.78 fps`; `0` if unmeasured). |
| `66` | 2 | `u16` | `video_fps_x100` | Rolling on-device SD video-recording FPS in centi-Hz (`0` if no video recorded since boot/reset). |
| `68` | 2 | `u16` | `stream_fps_x100` | Rolling active USB push-stream FPS in centi-Hz (`0` when streaming is off). |
| `70` | 1 | `u8` | `perf_mode` | Mode represented by stage timings: `0` = standalone live, `1` = video recording, `2` = serial stream, `3` = connected idle. |
| `71` | 1 | `u8` | `flags` | `bit 0` = `display_dma_active`, `bit 1` = `double_buf_active`, `bit 2` = `vospi_crc_check_enabled`. |
| `72` | 1 | `u8` | `display_spi_mhz` | Configured ILI9341 LCD `SPI` clock in MHz (e.g., `25`, `40`, `60`). |
| `73` | 1 | `u8` | `lepton_spi_mhz` | Configured FLIR Lepton VoSPI `SPI1` clock in MHz (e.g., `16`, `20`, `25`). |
| `74` | 2 | `u16` | `cpu_mhz` | Actual Teensy 4.1 ARM clock in MHz (e.g., `450`). |
| `76` | 4 | `f32` | `mcu_temp_c` | Teensy 4.1 internal die temperature (`tempmonGetTemp()`, °C). |
| `80` | 4 | `u32` | `frame_period_us` | Rolling mean frame period (µs) for `perf_mode` (microsecond-precision complement to `*_fps_x100`). |

#### `GET_RAD_PARAMS_REQ (0x1A)` (0 B) / `GET_RAD_PARAMS_RESP (0x5A)` & `SET_RAD_PARAMS_REQ (0x1B)` / `SET_RAD_PARAMS_RESP (0x5B)` — 18 bytes

Maps directly to FLIR Lepton CCI `LEP_CID_RAD_FLUX_LINEAR_PARAMS` (`0x4EBC` GET / `0x4EBD` SET, 8 × `uint16_t` = 16 bytes) so firmware passes values through to the sensor with zero floating-point rounding loss. Works alongside app-side global/per-ROI emissivity overrides.

**Fixed-point encoding:**
- **Q3.13 unitless coefficients (`scene_emissivity`, `tau_window`, `tau_atm`, `refl_window`):** `raw = round(value * 8192.0)` where `8192 = 1.0000`, `82 ≈ 0.0100`.
- **Centi-Kelvin temperatures (`t_bkg_ck`, `t_window_ck`, `t_atm_ck`, `t_refl_ck`):** `raw = round((temp_c + 273.15) * 100.0)` where `29515 = 295.15 K = 22.00 °C`.

| Offset | Size | Type | Field | Lepton SDK Field | Valid Range | Default | Description |
|---:|---:|---|---|---|---|---|---|
| `0` | 2 | `u16` | `scene_emissivity` | `sceneEmissivity` | `82..8192` (`0.01..1.00`) | `8192` (`1.0`) | Target object emissivity (Q3.13). |
| `2` | 2 | `u16` | `t_bkg_ck` | `TBkgK` | `0..65535` | `29515` (`22.00 °C`) | Reflected background temperature off the target (centi-Kelvin). |
| `4` | 2 | `u16` | `tau_window` | `tauWindow` | `82..8192` (`0.01..1.00`) | `8192` (`1.0`) | External IR window transmission (Q3.13; `8192` = no window). |
| `6` | 2 | `u16` | `t_window_ck` | `TWindowK` | `0..65535` | `29515` (`22.00 °C`) | External window temperature (centi-Kelvin). |
| `8` | 2 | `u16` | `tau_atm` | `tauAtm` | `82..8192` (`0.01..1.00`) | `8192` (`1.0`) | Atmospheric transmission (Q3.13; `8192` = short-range). |
| `10` | 2 | `u16` | `t_atm_ck` | `TAtmK` | `0..65535` | `29515` (`22.00 °C`) | Atmospheric temperature (centi-Kelvin). |
| `12` | 2 | `u16` | `refl_window` | `reflWindow` | `0..8192` (`0.00..1.00`) | `0` (`0.0`) | External window reflectivity (Q3.13). MUST satisfy `tau_window + refl_window <= 8192`. |
| `14` | 2 | `u16` | `t_refl_ck` | `TReflK` | `0..65535` | `29515` (`22.00 °C`) | Ambient temperature reflected by the external window (centi-Kelvin). |
| `16` | 1 | `u8` | `flags` | — | Bitmask | `0` | On `SET_RAD_PARAMS_REQ`: `bit 0` = `persist_to_eeprom`, `bit 1` = `reset_to_defaults` (ignores offsets `0..15` and restores factory defaults). On `RESP`: `bit 0` = `supported`, `bit 1` = `persisted_in_eeprom`. |
| `17` | 1 | `u8` | `reserved` | — | `0` | `0` | Must be `0`. |

> **Important for app-side per-ROI emissivity:** When the desktop app performs per-ROI emissivity/reflected-temperature correction in post-processing, leave Lepton CCI parameters at factory defaults (`scene_emissivity = 8192`, `tau_window = 8192`, `tau_atm = 8192`, `refl_window = 0`) so raw pixels represent uncompensated apparent blackbody temperature ($T_{\text{app}}$). When the user wants on-device LCD + `.DAT` temperatures compensated for a single scene emissivity, send `SET_RAD_PARAMS_REQ` with the desired `scene_emissivity` and `t_bkg_ck` (keeping `tau_window = 8192`, `tau_atm = 8192`, `refl_window = 0` for an open lens).

---

### 4.5 Asynchronous Events (`0xC0–0xC5`, Camera → Host)

Events are unsolicited packets carrying the camera's own monotonic `seq` counter and `status_or_sub = 0`. Hosts MUST NOT assume event `seq` shares a namespace with host request `seq`.

| ID | Name | Payload Size | Exact LE Payload Layout |
|---:|---|---:|---|
| `0xC0` | `EVT_BUTTON` | `4 B` | `0: action: u8` (`0` short_press, `1` long_press, `2` down, `3` up), `1: reserved: u8`, `2..3: duration_ms: u16`. Replaces v1's frame-piggybacked capture triggers. |
| `0xC1` | `EVT_TOUCH` | `6 B` | `0: action: u8` (`0` tap, `1` long_touch), `1: reserved: u8`, `2..3: x: u16`, `4..5: y: u16` (display coordinates `0..319`, `0..239`). |
| `0xC2` | `EVT_BATTERY` | `6 B` | `0: bat_pct: i8` (`-1..100`), `1: charging: u8` (`0/1`), `2: low_warning: u8` (`1` if `<= 20%`), `3: reserved: u8`, `4..5: bat_mv: u16`. Emitted on charge-state change, 5% step, or low-battery threshold. |
| `0xC3` | `EVT_FFC` | `4 B` | `0: state: u8` (`0` started, `1` completed, `2` error), `1: reserved: u8`, `2..3: duration_ms: u16` (`0` on `started`, elapsed ms on `completed`). |
| `0xC4` | `EVT_SESSION_EXIT` | `2 B` | `0: reason: u8` (`0` user_touch, `1` heartbeat_timeout, `2` reboot_bootloader, `3` usb_disconnect), `1: reserved: u8`. |
| `0xC5` | `EVT_PERF` | `84 B` | Identical to `PerfTelemetry` (`GET_PERF_RESP`, §4.4). Emitted every `STREAM_CTRL.perf_interval_s` seconds when non-zero. |

---

### 4.6 SD Filesystem (`0xE0–0xED`)

Protocol v2 SD access replaces MTP reboot mode as the primary file transfer path on macOS, Windows, and Linux.

**Path Rules (Mandatory Root-Jail Validation):**
- All paths are UTF-8, forward-slash (`/`) separated, root-relative, without trailing slash except root `"/"` itself (e.g., `"/"`, `"/20261125103045.DAT"`, `"/20261125103045/000000.DAT"`).
- `1 <= path_len <= 128`.
- Firmware MUST reject any path that does not start with `'/'`, contains `'\'`, contains NUL (`0x00`), or contains `".."` as any path segment with `ERR_FORBIDDEN` (or `ERR_INVALID_ARG` for empty/oversized paths).

#### `FS_INFO_REQ (0xE0)` (0 B) / `FS_INFO_RESP (0xE8)` — 16 bytes
- `0: card_present: u8` (`0/1`), `1: fs_type: u8` (`0` none, `1` FAT16, `2` FAT32, `3` exFAT), `2..3: reserved: u16`
- `4..7: total_kb: u32`, `8..11: free_kb: u32`, `12..15: root_entry_count: u32`

#### `FS_LIST_REQ (0xE1)` / `FS_LIST_RESP (0xE9)` (Paginated)
- **Request (`8 + path_len` B):** `0..3: cursor: u32` (`0` for first page), `4..5: max_entries: u16` (`1..64`), `6..7: path_len: u16`, `8..: path: u8[path_len]`.
- **Response (`8 + sum(entries)` B):**
  - `0..3: next_cursor: u32` (`0xFFFFFFFF` when the final entry in the directory has been returned; otherwise pass this value as `cursor` in the next `FS_LIST_REQ`).
  - `4..5: entry_count: u16`, `6..7: reserved: u16`.
  - Followed by `entry_count` packed entries, each `10 + name_len` bytes:
    - `0..3: size: u32` (`0` for directories)
    - `4..7: mtime_epoch: u32` (Unix timestamp derived from FAT/exFAT modification date/time)
    - `8: is_dir: u8` (`0` file, `1` directory)
    - `9: name_len: u8` (`1..255`, basename only, no leading slash)
    - `10..: name: u8[name_len]` (UTF-8)

#### `FS_STAT_REQ (0xE2)` / `FS_STAT_RESP (0xEA)`
- **Request (`4 + path_len` B):** `0: flags: u8` (`bit 0 = compute_crc32`), `1: reserved: u8`, `2..3: path_len: u16`, `4..: path: u8[path_len]`.
- **Response (16 B):** `0..3: size: u32`, `4..7: mtime_epoch: u32`, `8..11: file_crc32: u32` (`0` if `crc_valid == 0`), `12: is_dir: u8`, `13: crc_valid: u8` (`0/1`), `14..15: reserved: u16`.

#### `FS_READ_REQ (0xE3)` / `FS_READ_DATA (0xEB)` (Windowed Chunked Transfer)
- **Request (`12 + path_len` B):**
  - `0..3: offset: u32` (starting byte offset in file)
  - `4..7: length: u32` (requested window size in bytes; `0` = read to EOF; hosts SHOULD request bounded windows of `65536` B for large files so control commands interleave cleanly)
  - `8..9: chunk_size: u16` (desired bytes per `FS_READ_DATA` packet, clamped to `512 .. HELLO_RESP.max_chunk_size`, default `8192`)
  - `10..11: path_len: u16`, `12..: path: u8[path_len]`
- **Response (`FS_READ_DATA (0xEB)`, 1 or more packets sharing the request's `seq`):**
  - Header `flags`: `FLAG_MORE_CHUNKS (0x04)` is **set** on every chunk except the final chunk of the requested window (or EOF); **cleared (`0`)** on the final chunk.
  - Payload (`8 + data_len` B): `0..3: offset: u32` (file byte offset of this chunk), `4..5: data_len: u16` (`0..max_chunk_size`), `6: eof: u8` (`1` if end of file reached), `7: reserved: u8`, `8..: data: u8[data_len]`.
  - On mid-transfer SD error, camera emits a final `FS_READ_DATA` with `FLAG_ERROR (0x02)` set, `FLAG_MORE_CHUNKS` cleared, and `status_or_sub = ERR_IO`.
  - Only one `FS_READ_REQ` may be active at a time; a second overlapping `FS_READ_REQ` returns `ERR_BUSY`.

#### `FS_DELETE_REQ (0xE4)` / `FS_DELETE_RESP (0xEC)`
- **Request (`4 + path_len` B):** `0: flags: u8` (`bit 0 = recursive_if_dir`), `1: reserved: u8`, `2..3: path_len: u16`, `4..: path: u8[path_len]`.
- **Response (4 B):** `0..3: deleted_count: u32`.
- Deleting `"/"` MUST return `ERR_FORBIDDEN`. Deleting a non-empty directory without `bit 0` returns `ERR_FORBIDDEN`.

#### `FS_ABORT_REQ (0xE5)` (2 B) / `FS_ABORT_RESP (0xED)` (0 B)
- Request: `0..1: target_seq: u16` (sequence number of active `FS_READ_REQ` to cancel, or `0` to cancel any active read).
- Response: `OK` (0 B). Camera immediately closes the open read file and stops emitting `FS_READ_DATA` packets for `target_seq`.

---

### 4.7 Firmware Update, Bootloader & Generic Errors (`0xF0–0xF1`, `0xFF`)

#### `ENTER_BOOTLOADER_REQ (0xF0)` — 4 bytes / `ENTER_BOOTLOADER_RESP (0xF1)` — 0 bytes
- **Request:** `0..3: magic: u32` — MUST equal **`0xB00710AD`** (wire bytes `AD 10 07 B0`). Any other value returns `ERR_BAD_MAGIC (0x0009)` without rebooting.
- **Firmware reboot sequence (verified in Teensyduino 1.62.0 `cores/teensy4/usb.c:241-257`):**
  1. Validate `payload_len == 4` and `magic == 0xB00710AD`.
  2. Stop Lepton VSYNC IRQ and wait for any active LCD DMA / SD write to complete.
  3. Show `"Firmware Update Mode..."` on LCD (if present).
  4. Send `ENTER_BOOTLOADER_RESP (0xF1, status = OK)` + `Serial.send_now(); delay(50);`.
  5. Invoke Teensy core `_reboot_Teensyduino_()` (`asm("bkpt #251")` on unlocked Teensy 4.1; NXP ROM jump on lockable boards).
  6. Device re-enumerates as PJRC HalfKay USB HID (`VID 0x16C0`, `PID 0x0478`, 1088-byte reports for Teensy 4.1).
- **Legacy v3.0.x Upgrade Fallback (134-baud CDC trigger):**
  - If a camera fails to answer `HELLO_REQ` (e.g., it is still running v1 firmware `<= 3.0.4`), the desktop updater can enter HalfKay without the hardware button by opening the CDC serial port at **134 baud** (`8N1`) and closing it. Teensyduino's USB ISR (`usb.c:841`) arms an 80-SOF (~10 ms) timer that calls `_reboot_Teensyduino_()`.

#### `GENERIC_ERROR_RESP (0xFF)` — `0..128` bytes
Sent whenever a request cannot be mapped to a dedicated response ID (e.g., `ERR_UNKNOWN_MSG`, `ERR_BAD_CRC` on a valid header, `FLAG_COMPRESSED` set).
- `flags` MUST have `FLAG_ERROR (0x02)` set.
- `seq` echoes the offending request's `seq`.
- `status_or_sub` holds the non-zero `StatusCode` (§3.2).
- `payload` is empty (`0 B`) or an optional UTF-8 diagnostic message (`<= 128 B`, no NUL terminator required).

---

## 5. Golden Byte Test Vectors

Both `firmware/3.0/tests_host/` (C++17) and `app/crates/thermocam-core` (Rust) MUST include unit tests verifying bit-exact encoding, decoding, and resynchronization against every vector below.

### 5.1 Checksum Reference Checks
- `crc8_smbus(b"123456789") == 0xF4`
- `crc32_ieee(b"123456789") == 0xCBF43926`

### 5.2 Complete Packet Vectors (Hex, Space-Separated)

1. **`HELLO_REQ` (`msg_type=0x01`, `seq=1`, `flags=0x00`, `status=0x0000`, `payload_len=8`)**
   - Payload: `host_proto_major=2, host_proto_minor=0, reserved=0, host_features=0x00000001`
   - `hdr_crc8 = 0xA4`, `crc32 = 0x47914F86` (28 bytes):
   ```text
   54 43 32 00 02 01 00 A4 01 00 00 00 08 00 00 00
   02 00 00 00 01 00 00 00 86 4F 91 47
   ```

2. **`PING_REQ` (`msg_type=0x03`, `seq=2`, `flags=0x00`, `status=0x0000`, `payload_len=8`)**
   - Payload: `host_time_us = 1_000_000` (`0x00000000000F4240`)
   - `hdr_crc8 = 0x41`, `crc32 = 0xD6ECF99F` (28 bytes):
   ```text
   54 43 32 00 02 03 00 41 02 00 00 00 08 00 00 00
   40 42 0F 00 00 00 00 00 9F F9 EC D6
   ```

3. **`ENTER_BOOTLOADER_REQ` (`msg_type=0xF0`, `seq=42`, `flags=0x00`, `status=0x0000`, `payload_len=4`)**
   - Payload: `magic = 0xB00710AD`
   - `hdr_crc8 = 0x9C`, `crc32 = 0x1C8ED10F` (24 bytes):
   ```text
   54 43 32 00 02 F0 00 9C 2A 00 00 00 04 00 00 00
   AD 10 07 B0 0F D1 8E 1C
   ```

4. **`SET_RAD_PARAMS_REQ` (`msg_type=0x1B`, `seq=7`, `flags=0x00`, `status=0x0000`, `payload_len=18`)**
   - Payload: `scene_emissivity=7782 (0.95)`, `t_bkg_ck=29515 (22.00 °C)`, `tau_window=8192`, `t_window_ck=29515`, `tau_atm=8192`, `t_atm_ck=29515`, `refl_window=0`, `t_refl_ck=29515`, `flags=1 (persist)`, `reserved=0`
   - `hdr_crc8 = 0x3E`, `crc32 = 0xA2288BAD` (38 bytes):
   ```text
   54 43 32 00 02 1B 00 3E 07 00 00 00 12 00 00 00
   66 1E 4B 73 00 20 4B 73 00 20 4B 73 00 00 4B 73
   01 00 AD 8B 28 A2
   ```

5. **`GENERIC_ERROR_RESP` (`msg_type=0xFF`, `seq=99`, `flags=0x02`, `status=0x0002`, `payload_len=0`)**
   - `hdr_crc8 = 0x65`, `crc32 = 0x9640411B` (20 bytes):
   ```text
   54 43 32 00 02 FF 02 65 63 00 02 00 00 00 00 00
   1B 41 40 96
   ```

6. **`GOODBYE_REQ` (`msg_type=0x02`, `seq=100`, `flags=0x00`, `status=0x0000`, `payload_len=0`)**
   - `hdr_crc8 = 0x41`, `crc32 = 0x48F2C0F0` (20 bytes):
   ```text
   54 43 32 00 02 02 00 41 64 00 00 00 00 00 00 00
   F0 C0 F2 48
   ```

7. **Resync Stream Test (38 bytes — 2 garbage bytes + 16-byte fake `"TC2\0"` header with bad `hdr_crc8 = 0xFF` & bogus `payload_len = 65536` + valid `GOODBYE_REQ`):**
   - Parser MUST reject the fake header immediately on `hdr_crc8` mismatch (incrementing `usb_rx_errors` by 1) and decode `GOODBYE_REQ (seq=100)` cleanly when fed byte-by-byte or in arbitrary chunks:
   ```text
   AA BB 54 43 32 00 02 01 00 FF 00 00 00 00 00 00
   01 00 54 43 32 00 02 02 00 41 64 00 00 00 00 00
   00 00 F0 C0 F2 48
   ```

---

## 6. Reference Implementation Notes & Throughput Budget

### 6.1 Explicit Wire Serialization (No Unaligned Struct Casts)
- **Never** cast raw byte buffers directly to C++ `struct` pointers (`reinterpret_cast<const Header*>(buf)`) or use `#[repr(C, packed)]` pointer dereferences in Rust. Unaligned accesses and compiler padding differences cause subtle bugs and UB.
- **C++17 (`firmware/3.0/src/core/tc_proto_v2.*`):**
  - Keep `tc_proto_v2.h/.cpp` 100% free of `<Arduino.h>` (`<cstdint>`, `<cstddef>`, `<cstring>` only) so the identical file compiles under both GCC 15.2 (Teensy 4.1) and host `clang++`/`g++` CI tests.
  - Use tiny `inline` little-endian `read_u16_le` / `write_u16_le` / `read_u32_le` / `write_u32_le` / `read_u64_le` / `write_u64_le` / `read_f32_le` / `write_f32_le` helpers (`memcpy` into scalar + shift on big-endian if ever ported).
  - For zero-copy `FRAME_RAW` TX on Teensy 4.1 (little-endian Cortex-M7): compute streaming `crc32` across `(header[16], meta[64], pixel_buf[w*h*2], points[n*6])` and issue 3–4 bulk `Serial.write()` calls + 4-byte `crc32` + `Serial.send_now()`. Zero dynamic allocation and zero extra 38 KB copy buffer.
  - Teensy 4.1 RX buffer only needs `MAX_RX_PAYLOAD = 512` bytes (largest host→camera packet is `SET_TEMP_POINTS_REQ` = `2 + 96*4 = 386` B; reject larger host requests early if `payload_len > MAX_RX_PAYLOAD` with `ERR_INVALID_ARG`!).
- **Rust (`app/crates/thermocam-core`):**
  - Use explicit `u16::from_le_bytes` / `u32::from_le_bytes` / `f32::from_le_bytes` (or `bytes::Buf` / `BufMut`) with bounds-checked slicing, returning typed `ProtocolError` on truncated payloads.
  - Use `crc32fast::Hasher` for IEEE CRC-32 and a 256-entry `const` lookup table for CRC-8/SMBUS.
  - On `FRAME_RAW`, pass the verified payload slice (`&payload[64 .. 64 + w*h*2]`) directly into `u16` pixel conversion (`chunks_exact(2).map(u16::from_le_bytes)`) and deliver raw `u16` + 64-byte metadata over Tauri v2 `Channel<InvokeResponseBody::Raw>` with zero JSON/Base64 overhead.

### 6.2 Throughput & Latency Budget (Teensy 4.1 USB HS 480 Mbit/s)

Practical Teensy 4.1 USB CDC bulk throughput is **10–25 MB/s** (`80–200 Mbit/s`).

| Workload | Packet Payload | Wire Size (+20 B) | Rate | Bandwidth | USB TX Time @ 12 MB/s |
|---|---:|---:|---:|---:|---:|
| `FRAME_RAW` (Lepton 3.5, 160×120) | `38,464 B` | `38,484 B` | `8.78 Hz` | **`338 KB/s`** | **`~3.2 ms`** |
| `FRAME_RAW` (Lepton 2.5, 80×60) | `9,664 B` | `9,684 B` | `8.78 Hz` | **`85 KB/s`** | **`~0.8 ms`** |
| `FRAME_RENDERED` (320×240 RGB565) | `153,616 B` | `153,636 B` | `8.78 Hz` | **`1.35 MB/s`** | **`~12.8 ms`** |
| `RAW + RENDERED` simultaneous | `192,080 B` | `192,120 B` | `8.78 Hz` | **`1.69 MB/s`** | **`~16.0 ms`** |
| SD `FS_READ` (8 KB chunks, `.DAT` 38.8 KB) | `8,200 B × 5` | `~38.9 KB` | On-demand | `5–15 MB/s` | **`~4–8 ms`** |
| SD `FS_READ` (`.BMP` 614 KB) | `8,200 B × 75` | `~616 KB` | On-demand | `5–15 MB/s` | **`~50–120 ms`** |
| Control round-trip (`GET_CONFIG`, `PING`) | `0–36 B` | `20–56 B` | Concurrent | Negligible | **`< 0.5 ms`** (target `< 2 ms` end-to-end) |

Even simultaneous `FRAME_RAW + FRAME_RENDERED` uses **< 15%** of Teensy 4.1's conservative USB HS bulk bandwidth, leaving > 85% headroom for concurrent `< 2 ms` control commands and multi-MB/s SD card downloads without dropping thermal frames.
