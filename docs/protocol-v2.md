# DIY-Thermocam USB Protocol v2.0

Binary USB protocol used by Firmware 4.0 ([`firmware/src/core/tc_proto_v2.*`](https://github.com/maxritter/diy-thermocam/tree/master/firmware/src/core)) and Desktop Studio ([`app/crates/thermocam-core`](https://github.com/maxritter/diy-thermocam/tree/master/app/crates/thermocam-core)). All multi-byte integers and IEEE 754 floats are **little-endian (LE)**.

## At a Glance

- **Transport:** USB 2.0 High-Speed CDC ACM (VID `0x16C0`, PID `0x0483` or `0x04D5`). Open at `115200 8N1`, no flow control, **DTR = 1**.
- **Never open at 134 baud** during normal use—Teensyduino treats 134 baud as a HalfKay bootloader reboot trigger.
- **Full-duplex:** The camera pushes live thermal frames (`8.77 fps`) and button/touch/battery events while answering control and SD requests in under `1 ms`.
- **Framing:** 16-byte header + `0..163,840`-byte payload + 4-byte CRC-32 trailer (`20 B` overhead).
- **Session watchdog:** Send `PING_REQ` (or any valid packet) at least every `5000 ms` (recommended every `1–2 s`). Dropping DTR, long-touching the camera screen, or sending `GOODBYE_REQ` returns the camera to standalone live mode.

```text
Host                                      Camera (Teensy 4.1)
  |--- HELLO_REQ (0x01) ------------------>| Enters USB v2 session
  |<-- HELLO_RESP (0x41) ------------------| Returns sensor size, caps, UID
  |--- STREAM_CTRL_REQ (0x10, mode=1) ---->| Starts raw push stream
  |<-- STREAM_CTRL_RESP (0x50) ------------|
  |<== FRAME_RAW (0x80) @ 8.77 fps ========| Async frames + EVT_* events
  |--- GET_CONFIG / FS_READ / PING ------->| Concurrent (< 1 ms latency)
  |<-- *_RESP / FS_READ_DATA / PONG -------|
  |--- GOODBYE_REQ (0x02) ---------------->| Stops stream, restores LCD
  |<-- GOODBYE_RESP (0x42) ----------------|
```

---

## Packet Frame and Checksums

```text
 0      4     5      6       7        8     10            12            16              16+N
+------+-----+------+-------+--------+-----+-------------+-------------+---------------+-------+
| TC2\0| ver | type | flags | hdrCrc | seq | status/sub  | payload_len | payload (N B) | crc32 |
+------+-----+------+-------+--------+-----+-------------+-------------+---------------+-------+
|<------------------------- 16-byte header --------------------------->|               |< 4 B >|
|<--------------------------- covered by trailing IEEE CRC-32 ------------------------>|
```

### Header (16 bytes, LE)

| Offset | Type | Field | Rules |
|---:|---|---|---|
| `0` | `u8[4]` | `magic` | Fixed bytes `54 43 32 00` (`"TC2\0"`). |
| `4` | `u8` | `version` | Must be `0x02`. |
| `5` | `u8` | `msg_type` | Message ID (table below). |
| `6` | `u8` | `flags` | `0x01` ACK_REQ, `0x02` ERROR, `0x04` MORE_CHUNKS, `0x08` COMPRESSED (must be `0` in v2.0). |
| `7` | `u8` | `hdr_crc8` | **CRC-8/SMBUS** over the other 15 header bytes (`[0..6]` + `[8..15]`). |
| `8` | `u16` | `seq` | Host increments per request and camera echoes it. Camera uses its own counter for frames and events. |
| `10` | `u16` | `status_or_sub` | `StatusCode` in responses (`0x0000 = OK`); `0` in requests, streams, and events. |
| `12` | `u32` | `payload_len` | `0 .. 163840` (`MAX_PAYLOAD_LEN`). |

### Checksums and Resync

- **`hdr_crc8` (CRC-8/SMBUS):** poly `0x07`, init `0x00`, no reflection, xorout `0x00` (`"123456789"` → `0xF4`). Lets the parser reject false `"TC2\0"` bytes inside pixel streams immediately.
- **`crc32` (IEEE 802.3):** poly `0x04C11DB7` (reflected `0xEDB88320`), init/xorout `0xFFFFFFFF`, reflected (`"123456789"` → `0xCBF43926`). Matches Python `zlib.crc32` and Rust `crc32fast`.
- **Resync rule:** If `version`, `payload_len`, or `hdr_crc8` fails after reading 16 header bytes, shift `header[1..16]` left by 1 byte and rescan those 15 bytes for `"TC2\0"` before reading new bytes. Abort an incomplete payload after `500 ms` idle.
- **Backpressure:** Control responses and events take priority over stream frames. If USB TX cannot accept a frame within `15 ms`, the camera drops it whole before starting TX (never truncates mid-packet) and increments `dropped_frames`.

### Status Codes (`status_or_sub`)

When `status_or_sub != 0`, `FLAG_ERROR (0x02)` is set and the payload is empty or a UTF-8 diagnostic string (`<= 128 B`).

| Code | Constant | Meaning |
|---:|---|---|
| `0x0000` | `OK` | Success (`FLAG_ERROR` clear). |
| `0x0001` | `ERR_BAD_CRC` | Payload CRC-32 mismatch (sent via `GENERIC_ERROR_RESP`). |
| `0x0002` | `ERR_UNKNOWN_MSG` | Unknown `msg_type` (sent via `GENERIC_ERROR_RESP`). |
| `0x0003` | `ERR_INVALID_ARG` | Bad length, out-of-range value, or invalid path. Atomic—no state changes. |
| `0x0004` | `ERR_BUSY` | Subsystem busy (FFC or `FS_READ` already active). |
| `0x0005` | `ERR_IO` | Lepton I2C/SPI or SD card I/O failure. |
| `0x0006` | `ERR_NOT_FOUND` | SD file or directory not found. |
| `0x0007` | `ERR_FORBIDDEN` | Path traversal (`..`, `\`), deleting `"/"`, or FFC in manual mode without force flag. |
| `0x0008` | `ERR_UNSUPPORTED` | Unsupported capability or `FLAG_COMPRESSED` set. |
| `0x0009` | `ERR_BAD_MAGIC` | Bootloader safety token mismatch in `ENTER_BOOTLOADER_REQ`. |
| `0x000A` | `ERR_NO_SD` | SD card absent, unmounted, or `< 1 MB` free. |

---

## Message Catalog

Control requests `0x01..0x3F` map to responses `req | 0x40`. SD requests `0xE0..0xE5` map to responses `req | 0x08`.

| Group | Req ID | Request | Resp / Push ID | Response / Push | Direction |
|---|---:|---|---:|---|---|
| Lifecycle | `0x01` | `HELLO_REQ` | `0x41` | `HELLO_RESP` | Host ↔ Cam |
| Lifecycle | `0x02` | `GOODBYE_REQ` | `0x42` | `GOODBYE_RESP` | Host ↔ Cam |
| Lifecycle | `0x03` | `PING_REQ` | `0x43` | `PONG_RESP` | Host ↔ Cam |
| Streaming | `0x10` | `STREAM_CTRL_REQ` | `0x50` | `STREAM_CTRL_RESP` | Host ↔ Cam |
| Control | `0x11` | `GET_CONFIG_REQ` | `0x51` | `GET_CONFIG_RESP` | Host ↔ Cam |
| Control | `0x12` | `SET_CONFIG_REQ` | `0x52` | `SET_CONFIG_RESP` | Host ↔ Cam |
| Control | `0x13` | `SET_TIME_REQ` | `0x53` | `SET_TIME_RESP` | Host ↔ Cam |
| Control | `0x14` | `TRIGGER_FFC_REQ` | `0x54` | `TRIGGER_FFC_RESP` | Host ↔ Cam |
| Control | `0x15` | `GET_TEMP_POINTS_REQ` | `0x55` | `GET_TEMP_POINTS_RESP` | Host ↔ Cam |
| Control | `0x16` | `SET_TEMP_POINTS_REQ` | `0x56` | `SET_TEMP_POINTS_RESP` | Host ↔ Cam |
| Control | `0x17` | `SAVE_CAPTURE_REQ` | `0x57` | `SAVE_CAPTURE_RESP` | Host ↔ Cam |
| Control | `0x18` | `GET_DIAGNOSTICS_REQ` | `0x58` | `GET_DIAGNOSTICS_RESP` | Host ↔ Cam |
| Control | `0x19` | `GET_PERF_REQ` | `0x59` | `GET_PERF_RESP` | Host ↔ Cam |
| Radiometry | `0x1A` | `GET_RAD_PARAMS_REQ` | `0x5A` | `GET_RAD_PARAMS_RESP` | Host ↔ Cam |
| Radiometry | `0x1B` | `SET_RAD_PARAMS_REQ` | `0x5B` | `SET_RAD_PARAMS_RESP` | Host ↔ Cam |
| Stream | — | — | `0x80` | `FRAME_RAW` | Cam → Host |
| Stream | — | — | `0x81` | `FRAME_RENDERED` | Cam → Host |
| Event | — | — | `0xC0–0xC5` | `EVT_BUTTON`, `TOUCH`, `BATTERY`, `FFC`, `SESSION_EXIT`, `PERF` | Cam → Host |
| SD FS | `0xE0–0xE5` | `FS_INFO`, `LIST`, `STAT`, `READ`, `DELETE`, `ABORT` | `0xE8–0xED` | Matching `FS_*_RESP` / chunked `FS_READ_DATA` | Host ↔ Cam |
| System | `0xF0` | `ENTER_BOOTLOADER_REQ` | `0xF1` | `ENTER_BOOTLOADER_RESP` | Host ↔ Cam |
| Error | — | — | `0xFF` | `GENERIC_ERROR_RESP` | Cam → Host |

---

## Payload Layouts

### Lifecycle (`0x01–0x03`, `0x41–0x43`)

- **`HELLO_REQ (0x01)` (8 B):** `0: host_proto_major: u8 (2)`, `1: host_proto_minor: u8 (0)`, `2..3: reserved (0)`, `4..7: host_features: u32` (`bit 0` keep LCD on, `bit 1` supports perf events).
- **`GOODBYE_REQ (0x02)` / `GOODBYE_RESP (0x42)` (0 B):** Stops streaming, flushes USB, and restores standalone live mode.
- **`PING_REQ (0x03)` (8 B) → `PONG_RESP (0x43)` (16 B):** Request sends `0..7: host_time_us: u64`; response echoes `0..7: host_time_us` and appends `8..15: device_uptime_us: u64`.

**`HELLO_RESP (0x41)` (64 B):**

| Offset | Type | Field | Notes |
|---:|---|---|---|
| `0` | `u8, u8` | `proto_major`, `proto_minor` | `2`, `0` |
| `2` | `u16` | `fw_version` | Integer version (e.g. `400` for `4.0.0`). |
| `4` | `u8[32]` | `fw_build_str` | Null-padded UTF-8 build string (e.g. `"4.0.0 (2026-11-25)"`). |
| `36` | `u8, u8` | `hw_version`, `lepton_type` | `hw`: `3` (Teensy 4.1 boards, always sent by Firmware 4.0), `2` (older Teensy 3.x cameras). `lepton`: `0` = 2.5, `1` = 3.5/3.1R, `255` = absent. |
| `38` | `u16, u16` | `sensor_w`, `sensor_h` | `160 × 120` or `80 × 60`. |
| `42` | `u16, u16` | `display_w`, `display_h` | `320 × 240`, or `0 × 0` if headless. |
| `46` | `u8, u8` | `diagnostic_mask`, `reserved` | Bits `0..4` (`1 = OK`): display, touch, SD, battery, Lepton; bits `5..7` = `1`. |
| `48` | `u64` | `teensy_uid` | Unique 64-bit Teensy 4.1 hardware ID. |
| `56` | `u32` | `capabilities` | Bitmask: `0` raw stream, `1` display stream, `2` SD FS, `3` perf, `4` bootloader, `5` rad params, `6` gain mode, `7` hot/cold, `8` RTC sync, `9` keep LCD on. |
| `60` | `u16, u16` | `max_chunk_size`, `heartbeat_timeout_ms` | Defaults: `8192` B, `5000` ms. |

---

### Streaming (`0x10`, `0x50`, `0x80–0x81`)

- **`STREAM_CTRL_REQ (0x10)` / `STREAM_CTRL_RESP (0x50)` (8 B):**
  - `0: mode: u8` (`0` off, `1` raw, `2` rendered, `3` both)
  - `1: keep_lcd_on: u8` (`0/1`), `2: include_temp_points: u8` (`0/1`), `3: perf_interval_s: u8` (`0..60`)
  - `4..5: max_fps_x10: u16` (`0` = native ~8.78 Hz; e.g. `50` = 5.0 Hz), `6..7: reserved (0)`

**`FRAME_RAW (0x80)` (`64 + w*h*2 + temp_point_count*6` B):**

| Offset | Type | Field | Notes |
|---:|---|---|---|
| `0` | `u32` | `frame_seq` | Monotonic frame counter since boot. |
| `4` | `u64` | `timestamp_us` | Monotonic Teensy uptime (µs) at frame completion. |
| `12` | `u32` | `rtc_epoch` | Unix seconds from Teensy RTC. |
| `16` | `u32` | `dropped_frames` | Dropped frames since streaming started. |
| `20` | `u16, u16` | `width`, `height` | `160 × 120` or `80 × 60`. |
| `24` | `u8, u8, u8` | `pixel_format`, `gain_mode`, `ffc_state` | `pixel_format = 0` (`u16` LE). `gain`: `0` high (`0.01 K/cnt`), `1` low (`0.1 K/cnt`). `ffc`: `0` idle, `1` running, `2` completed. |
| `27` | `u8` | `flags` | `bit 0` limits_locked, `bit 1` auto_mode, `bit 2` temp_points_present, `bit 3` rot_vert, `bit 4` rot_horiz. |
| `28` | `u16 × 6` | `min_raw`, `max_raw`, `min_x`, `min_y`, `max_x`, `max_y` | Active bounds and hottest/coldest pixel coordinates. |
| `40` | `f32 × 4` | `spot_temp_c`, `cal_offset`, `cal_slope`, `fpa_temp_c` | Always °C on wire (`T_C = raw * cal_slope + cal_offset`, offset `-273.15f`). `fpa_temp_c` is `NaN` if unpolled. |
| `56` | `i8, u8` | `bat_pct`, `usb_power` | `bat_pct`: `0..100` or `-1` (USB-only). `usb_power`: `0/1`. |
| `58` | `u16` | `temp_point_count` | `0..96` (only non-zero when `flags & 0x04`). |
| `60` | `u8[4]` | `reserved` | `0`. |
| `64` | `u16[w*h]` | `pixels` | Row-major raw Lepton T-Linear counts (`38,400 B` for 160×120). |
| `64+2wh` | `{u16,u16,u16}[]` | `temp_points` | Optional `temp_point_count` × 6-byte `{ x, y, raw }` records. |

- **`FRAME_RENDERED (0x81)` (`153,616` B):** `0..3: frame_seq: u32`, `4..11: timestamp_us: u64`, `12..13: width: u16 (320)`, `14..15: height: u16 (240)`, `16..153615: u16[76800]` LE RGB565 pixels.

---

### Configuration, Control & Radiometry (`0x11–0x1B`, `0x51–0x5B`)

**`DeviceConfig` (32 B)** — returned by `GET_CONFIG_RESP (0x51)` and `SET_CONFIG_RESP (0x52)`. `SET_CONFIG_REQ (0x12)` is 36 B: `0: persist: u8 (0=RAM, 1=EEPROM)`, `1..3: reserved`, `4..35: DeviceConfig`. Validation is atomic.

| Offset | Type | Field | Valid Values |
|---:|---|---|---|
| `0` | `u8` | `config_ver` | `1` |
| `1..5` | `u8 × 5` | `color_scheme`, `text_color`, `temp_format`, `filter_type`, `min_max_points` | Scheme `0..18`; text `0..4` (W/K/R/G/B); format `0` °C / `1` °F; filter `0` none / `1` gauss / `2` box; min/max `0..3` (off/min/max/both). |
| `6..9` | `u8 × 4` | `rotation_vert`, `rotation_horiz`, `limits_mode`, `shutter_mode` | Rotations `0/1`; limits `0` auto / `1` locked / `2` manual; shutter `0` manual / `1` auto. |
| `10..13` | `u8 × 4` | `lepton_gain_mode`, `hot_cold_mode`, `hot_cold_color`, `screen_off_time` | Gain `0` high / `1` low; hot/cold `0` off / `1` cold / `2` hot; color `0..4`; timeout `0` off / `1` 5 m / `2` 20 m. |
| `14` | `u16` | `overlay_flags` | Bits `0..6`: spot, colorbar, battery, time, date, storage, convert_bmp. |
| `16..20` | `u16, u16, i16` | `manual_min_raw`, `manual_max_raw`, `hot_cold_level_c10` | `manual_min < manual_max` when `limits_mode == 2`; hot/cold threshold in `0.1 °C`. |
| `22` | `u8` | `min_max_preset` | `0` temporary, `1..3` EEPROM presets. `23..31`: reserved (`0`). |

**Other control commands:**

| Command | Request Payload | Response Payload |
|---|---|---|
| `SET_TIME (0x13/0x53)` | `0..3: rtc_epoch: u32` (Unix seconds) | Echoes `rtc_epoch: u32` |
| `TRIGGER_FFC (0x14/0x54)` | `0: flags: u8` (`bit 0` = force in manual mode) | `0: ffc_status: u8 (0)`, `1: reserved` (+ `EVT_FFC` events) |
| `GET_TEMP_POINTS (0x15/0x55)` | `0 B` | `0..1: count: u16 (0..96)`, then `count × {x:u16, y:u16, raw:u16}` |
| `SET_TEMP_POINTS (0x16/0x56)` | `0..1: count: u16 (0..96)`, then `count × {x:u16, y:u16}` | `0..1: count: u16` |
| `SAVE_CAPTURE (0x17/0x57)` | `0: flags: u8` (`b0` save_bmp, `b1` override_cfg, `b2` fresh_frame), `1..3: 0` | `0..3: dat_size: u32`, `4..7: bmp_size: u32`, `8..39: stem: u8[32]` |
| `GET_DIAGNOSTICS (0x18/0x58)` | `0 B` | `80 B`: `0: mask`, `1: bat_pct: i8`, `2: usb`, `3: sd_fs (0..3)`, `4: bat_mv: u16`, `6: cpu_mhz: u16`, `8: mcu_temp_c: f32`, `12: sd_total_kb: u32`, `16: sd_free_kb: u32`, `20: uptime_us: u64`, `28: free_dtcm: u32`, `32: free_ocram: u32`, `36: fpa_temp_c: f32`, `40..71: lepton_part_num[32]`, `72..79: 0` |
| `GET_PERF (0x19/0x59)` | `0: flags: u8` (`b0` reset_counters, `b1` last_video_snapshot) | `84 B` `PerfTelemetry` (same layout as `EVT_PERF`, below) |
| `GET/SET_RAD_PARAMS (0x1A/0x1B)` | `GET`: `0 B`; `SET`: `18 B` (below) | `18 B` active Lepton CCI `0x4EBC`/`0x4EBD` flux-linear parameters |

**`PerfTelemetry` (84 B, `GET_PERF_RESP` & `EVT_PERF`):**
- `0..39`: 10 × `u32` microsecond stage timings: `acquire`, `stats`, `filter`, `scale`, `colorize`, `overlay`, `display`, `sd_write`, `usb_tx`, `loop_total`.
- `40..63`: 6 × `u32` counters: `frame_count`, `frames_dropped`, `vospi_resyncs`, `vospi_crc_errors`, `usb_rx_errors`, `usb_crc_errors`.
- `64..69`: 3 × `u16` rolling FPS in `0.01 Hz`: `live_fps_x100`, `video_fps_x100`, `stream_fps_x100`.
- `70..75`: `perf_mode: u8` (`0` live, `1` video, `2` stream, `3` idle), `flags: u8` (`b0` dma, `b1` double_buf, `b2` vospi_crc), `display_spi_mhz: u8`, `lepton_spi_mhz: u8`, `cpu_mhz: u16`.
- `76..83`: `mcu_temp_c: f32`, `frame_period_us: u32`.

**`RadParams` (18 B, `0x1A`/`0x1B`):** Maps 1:1 to FLIR Lepton `LEP_CID_RAD_FLUX_LINEAR_PARAMS`. Unitless factors use Q3.13 (`8192 = 1.0`, `82 ≈ 0.01`); temperatures use centi-Kelvin (`(°C + 273.15) × 100`, default `29515 = 22.00 °C`).
- `0..15` (`8 × u16`): `scene_emissivity` (`82..8192`), `t_bkg_ck`, `tau_window` (`82..8192`), `t_window_ck`, `tau_atm` (`82..8192`), `t_atm_ck`, `refl_window` (`0..8192`, with `tau_window + refl_window <= 8192`), `t_refl_ck`.
- `16`: `flags: u8` (`SET`: `b0` persist_eeprom, `b1` reset_defaults; `RESP`: `b0` supported, `b1` persisted). `17`: reserved (`0`).
- For desktop per-ROI emissivity correction, keep sensor params at defaults (`8192, 29515, 8192, 29515, 8192, 29515, 0, 29515`) so raw pixels stay uncompensated apparent blackbody temperature.

---

### Async Events (`0xC0–0xC5`, Camera → Host)

| ID | Event | Size | Payload (LE) |
|---:|---|---:|---|
| `0xC0` | `EVT_BUTTON` | `4 B` | `0: action: u8` (`0` short, `1` long, `2` down, `3` up), `1: 0`, `2..3: duration_ms: u16` |
| `0xC1` | `EVT_TOUCH` | `6 B` | `0: action: u8` (`0` tap, `1` long_touch), `1: 0`, `2..3: x: u16`, `4..5: y: u16` |
| `0xC2` | `EVT_BATTERY` | `6 B` | `0: bat_pct: i8`, `1: charging: u8`, `2: low_warn: u8`, `3: 0`, `4..5: bat_mv: u16` |
| `0xC3` | `EVT_FFC` | `4 B` | `0: state: u8` (`0` started, `1` done, `2` error), `1: 0`, `2..3: duration_ms: u16` |
| `0xC4` | `EVT_SESSION_EXIT` | `2 B` | `0: reason: u8` (`0` touch, `1` timeout, `2` bootloader, `3` usb_disconnect), `1: 0` |
| `0xC5` | `EVT_PERF` | `84 B` | `PerfTelemetry` (emitted every `perf_interval_s` seconds) |

---

### SD Filesystem (`0xE0–0xED`)

Paths are UTF-8, root-relative (`1..128 B`, starting with `'/'`, no trailing `'/'` except `"/"`). Any path with `..`, `\`, or NUL is rejected with `ERR_FORBIDDEN`.

| Command | Request Payload | Response Payload |
|---|---|---|
| `FS_INFO (0xE0/0xE8)` | `0 B` | `16 B`: `0: present`, `1: fs_type (0..3)`, `2..3: 0`, `4: total_kb: u32`, `8: free_kb: u32`, `12: root_entries: u32` |
| `FS_LIST (0xE1/0xE9)` | `0: cursor: u32`, `4: max_entries: u16 (1..64)`, `6: path_len: u16`, `8..: path` | `0: next_cursor: u32` (`0xFFFFFFFF` = end), `4: count: u16`, `6: 0`, then packed `{size:u32, mtime:u32, is_dir:u8, name_len:u8, name[name_len]}` |
| `FS_STAT (0xE2/0xEA)` | `0: flags: u8` (`b0` compute_crc32), `1: 0`, `2: path_len: u16`, `4..: path` | `16 B`: `0: size: u32`, `4: mtime: u32`, `8: crc32: u32`, `12: is_dir`, `13: crc_valid`, `14..15: 0` |
| `FS_READ (0xE3/0xEB)` | `0: offset: u32`, `4: length: u32` (`0`=EOF; use `65536` windows), `8: chunk_size: u16` (`512..max_chunk`), `10: path_len: u16`, `12..: path` | 1+ `FS_READ_DATA` packets sharing `seq` (`FLAG_MORE_CHUNKS` clear on last): `0: offset: u32`, `4: data_len: u16`, `6: eof: u8`, `7: 0`, `8..: data`. Single active read (`ERR_BUSY` if overlapped). |
| `FS_DELETE (0xE4/0xEC)` | `0: flags: u8` (`b0` recursive_if_dir), `1: 0`, `2: path_len: u16`, `4..: path` | `4 B`: `deleted_count: u32` (`ERR_FORBIDDEN` for `"/"` or non-empty dir without `b0`) |
| `FS_ABORT (0xE5/0xED)` | `0..1: target_seq: u16` (`0` = any active read) | `0 B` (`OK`) |

---

### Bootloader & Generic Errors (`0xF0–0xF1`, `0xFF`)

- **`ENTER_BOOTLOADER_REQ (0xF0)` (4 B) → `ENTER_BOOTLOADER_RESP (0xF1)` (0 B):**
  - Request `0..3: magic: u32` must be **`0xB00710AD`** (`AD 10 07 B0`), else `ERR_BAD_MAGIC`.
  - Camera halts Lepton/DMA, sends `OK`, waits `50 ms`, and calls `_reboot_Teensyduino_()`. Device re-enumerates as PJRC HalfKay HID (`VID 0x16C0`, `PID 0x0478`, 1088-byte reports).
  - **Firmware 3.0.x upgrade fallback:** If `HELLO_REQ` times out, Desktop Studio opens the CDC port at **134 baud** (`8N1`) and closes it to enter HalfKay without the physical button.
- **`GENERIC_ERROR_RESP (0xFF)` (`0..128 B`):** Sent when a request has no dedicated response ID (`ERR_UNKNOWN_MSG`, `ERR_BAD_CRC`, `FLAG_COMPRESSED`). Sets `FLAG_ERROR`, echoes `seq`, and carries `StatusCode` plus an optional UTF-8 message.

---

<details>
<summary><b>Golden byte test vectors (7 cross-language C++17 & Rust vectors)</b></summary>

Checksums: `crc8_smbus(b"123456789") == 0xF4`, `crc32_ieee(b"123456789") == 0xCBF43926`.

1. **`HELLO_REQ`** (`type=0x01`, `seq=1`, `len=8`, `host_features=1`, `hdr_crc8=0xA4`, `crc32=0x47914F86`):
   ```text
   54 43 32 00 02 01 00 A4 01 00 00 00 08 00 00 00
   02 00 00 00 01 00 00 00 86 4F 91 47
   ```
2. **`PING_REQ`** (`type=0x03`, `seq=2`, `len=8`, `host_time_us=1_000_000`, `hdr_crc8=0x41`, `crc32=0xD6ECF99F`):
   ```text
   54 43 32 00 02 03 00 41 02 00 00 00 08 00 00 00
   40 42 0F 00 00 00 00 00 9F F9 EC D6
   ```
3. **`ENTER_BOOTLOADER_REQ`** (`type=0xF0`, `seq=42`, `len=4`, `magic=0xB00710AD`, `hdr_crc8=0x9C`, `crc32=0x1C8ED10F`):
   ```text
   54 43 32 00 02 F0 00 9C 2A 00 00 00 04 00 00 00
   AD 10 07 B0 0F D1 8E 1C
   ```
4. **`SET_RAD_PARAMS_REQ`** (`type=0x1B`, `seq=7`, `len=18`, `eps=0.95`, `persist=1`, `hdr_crc8=0x3E`, `crc32=0xA2288BAD`):
   ```text
   54 43 32 00 02 1B 00 3E 07 00 00 00 12 00 00 00
   66 1E 4B 73 00 20 4B 73 00 20 4B 73 00 00 4B 73
   01 00 AD 8B 28 A2
   ```
5. **`GENERIC_ERROR_RESP`** (`type=0xFF`, `seq=99`, `flags=0x02`, `status=0x0002`, `len=0`, `hdr_crc8=0x65`, `crc32=0x9640411B`):
   ```text
   54 43 32 00 02 FF 02 65 63 00 02 00 00 00 00 00
   1B 41 40 96
   ```
6. **`GOODBYE_REQ`** (`type=0x02`, `seq=100`, `len=0`, `hdr_crc8=0x41`, `crc32=0x48F2C0F0`):
   ```text
   54 43 32 00 02 02 00 41 64 00 00 00 00 00 00 00
   F0 C0 F2 48
   ```
7. **Resync stream** (2 garbage bytes + fake `"TC2\0"` header with bad `hdr_crc8=0xFF` & `len=65536` + valid `GOODBYE_REQ`):
   ```text
   AA BB 54 43 32 00 02 01 00 FF 00 00 00 00 00 00
   01 00 54 43 32 00 02 02 00 41 64 00 00 00 00 00
   00 00 F0 C0 F2 48
   ```

</details>

<details>
<summary><b>Bandwidth budget and serialization rules</b></summary>

Teensy 4.1 USB HS bulk throughput is **10–25 MB/s**. Even simultaneous `FRAME_RAW` + `FRAME_RENDERED` uses under **15%** of available bandwidth:

| Workload | Wire Size | Rate | Bandwidth | USB TX @ 12 MB/s |
|---|---:|---:|---:|---:|
| `FRAME_RAW` (160×120) | `38,484 B` | `8.78 Hz` | `338 KB/s` | `~3.2 ms` |
| `FRAME_RAW` (80×60) | `9,684 B` | `8.78 Hz` | `85 KB/s` | `~0.8 ms` |
| `FRAME_RENDERED` (320×240) | `153,636 B` | `8.78 Hz` | `1.35 MB/s` | `~12.8 ms` |
| SD `FS_READ` (`.DAT`, 8 KB chunks) | `~38.9 KB` | On demand | `5–15 MB/s` | `~4–8 ms` |
| Control round-trip (`PING` / `GET_CONFIG`) | `20–56 B` | Concurrent | Negligible | `< 0.5 ms` |

**Serialization rules:** Never cast raw buffers to packed structs. Use explicit little-endian byte helpers (`read_u16_le` / `u16::from_le_bytes`), keep `firmware/src/core/tc_proto_v2.*` free of `<Arduino.h>` for host unit tests, and reject host requests exceeding `MAX_RX_PAYLOAD = 512 B` early with `ERR_INVALID_ARG`.

</details>
