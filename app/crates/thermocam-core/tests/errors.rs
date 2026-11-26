use std::cell::Cell;
use std::io;
use std::time::Duration;

use thermocam_core::flasher::{
    classify_hid_open_error, run_flash_update, FlashError, FlashEvent, FlashFailureKind,
    FlashOptions, FlashUpdateOptions, MockHalfKayDevice,
};
use thermocam_core::protocol::transport::serialport_error_to_io;
use thermocam_core::protocol::{
    classify_connect_error, ConnectError, ConnectErrorKind, ProtocolError,
};

fn io_err(kind: io::ErrorKind, msg: &str) -> ProtocolError {
    ProtocolError::Io(io::Error::new(kind, msg))
}

#[test]
fn connect_errors_are_classified_by_io_kind_first() {
    let permission_kind = if cfg!(windows) {
        // Windows reports a port held by another program as "Access is denied".
        ConnectErrorKind::Busy
    } else {
        ConnectErrorKind::Permission
    };
    let table: Vec<(ProtocolError, ConnectErrorKind)> = vec![
        (
            io_err(io::ErrorKind::PermissionDenied, "x"),
            permission_kind,
        ),
        (
            io_err(io::ErrorKind::ResourceBusy, "x"),
            ConnectErrorKind::Busy,
        ),
        (
            io_err(io::ErrorKind::NotFound, "x"),
            ConnectErrorKind::NotFound,
        ),
        (
            io_err(io::ErrorKind::Other, "something odd"),
            ConnectErrorKind::Other,
        ),
        (ProtocolError::Timeout(1), ConnectErrorKind::Protocol),
        (
            ProtocolError::DeviceError {
                status: 2,
                message: "unknown".into(),
            },
            ConnectErrorKind::Protocol,
        ),
        (ProtocolError::BadHeaderCrc, ConnectErrorKind::Protocol),
    ];
    for (err, want) in table {
        let text = err.to_string();
        assert_eq!(classify_connect_error(&err), want, "{text}");
    }
}

#[test]
fn connect_errors_fall_back_to_the_known_strings() {
    let table = [
        ("Permission denied", ConnectErrorKind::Permission),
        ("Device or resource busy", ConnectErrorKind::Busy),
        (
            "Unable to acquire exclusive lock on serial port",
            ConnectErrorKind::Busy,
        ),
        ("No such file or directory", ConnectErrorKind::NotFound),
    ];
    for (text, want) in table {
        let err = ProtocolError::Io(io::Error::other(text));
        assert_eq!(classify_connect_error(&err), want, "{text}");
    }
}

#[test]
fn serialport_errors_keep_their_io_kind() {
    let e = serialport::Error::new(
        serialport::ErrorKind::Io(io::ErrorKind::PermissionDenied),
        "Permission denied",
    );
    let io = serialport_error_to_io(e);
    assert_eq!(io.kind(), io::ErrorKind::PermissionDenied);
    assert_eq!(io.to_string(), "Permission denied");

    let busy = serialport::Error::new(serialport::ErrorKind::NoDevice, "Device or resource busy");
    let err = ProtocolError::Io(serialport_error_to_io(busy));
    assert_eq!(classify_connect_error(&err), ConnectErrorKind::Busy);
}

#[test]
fn connect_error_serialises_as_kind_and_detail() {
    let err = ConnectError::from(&io_err(
        io::ErrorKind::NotFound,
        "No such file or directory",
    ));
    let json = serde_json::to_value(&err).unwrap();
    assert_eq!(json["kind"], "not_found");
    assert!(json["detail"].as_str().unwrap().contains("No such file"));
}

/// Intel HEX record with checksum.
fn record(kind: u8, addr: u16, data: &[u8]) -> String {
    let mut bytes = vec![data.len() as u8, (addr >> 8) as u8, addr as u8, kind];
    bytes.extend_from_slice(data);
    let sum: u8 = bytes.iter().fold(0u8, |a, b| a.wrapping_add(*b));
    bytes.push(sum.wrapping_neg());
    let hex: String = bytes.iter().map(|b| format!("{b:02X}")).collect();
    format!(":{hex}\n")
}

/// A valid three-block Teensy 4.1 image.
fn three_block_hex() -> String {
    let mut s = record(0x04, 0, &[0x60, 0x00]);
    let mut first = b"FCFB".to_vec();
    first.extend_from_slice(&[0u8; 12]);
    s += &record(0x00, 0x0000, &first);
    s += &record(0x00, 0x0400, &[0xAA; 16]);
    s += &record(0x00, 0x0800, &[0xBB; 16]);
    s += &record(0x01, 0, &[]);
    s
}

fn fast_options() -> FlashUpdateOptions {
    FlashUpdateOptions {
        wait_timeout: Duration::from_millis(40),
        poll_interval: Duration::from_millis(5),
        flash: FlashOptions {
            max_attempts: 2,
            first_block_retry_delay: Duration::ZERO,
            block_retry_delay: Duration::ZERO,
            first_block_extra_attempts: 0,
        },
    }
}

fn stages(events: &[FlashEvent]) -> Vec<String> {
    events
        .iter()
        .map(|e| {
            serde_json::to_value(e).unwrap()["stage"]
                .as_str()
                .unwrap()
                .to_string()
        })
        .collect()
}

#[test]
fn a_successful_update_reports_every_stage() {
    let hex = three_block_hex();
    let mut events = Vec::new();
    let mut device = Some(MockHalfKayDevice::default());
    let sha = run_flash_update(
        &hex,
        None,
        fast_options(),
        || {},
        || device.take().ok_or(FlashError::DeviceNotFound),
        |e| events.push(e),
    )
    .unwrap();
    assert_eq!(sha.len(), 64);
    assert_eq!(
        stages(&events),
        [
            "verified",
            "waiting_bootloader",
            "flashing",
            "flashing",
            "flashing",
            "rebooted"
        ]
    );
}

#[test]
fn a_write_failing_mid_way_reports_error_write() {
    let hex = three_block_hex();
    let mut events = Vec::new();
    let mut dev = MockHalfKayDevice::default();
    dev.fail_block_attempts.insert(1, 100);
    let mut device = Some(dev);
    let err = run_flash_update(
        &hex,
        None,
        fast_options(),
        || {},
        || device.take().ok_or(FlashError::DeviceNotFound),
        |e| events.push(e),
    )
    .unwrap_err();
    assert_eq!(err.kind, FlashFailureKind::Write);
    assert_eq!(
        stages(&events),
        ["verified", "waiting_bootloader", "flashing", "error"]
    );
    let last = serde_json::to_value(events.last().unwrap()).unwrap();
    assert_eq!(last["kind"], "write");
    assert!(last["detail"].as_str().unwrap().contains("block 1"));
}

#[test]
fn a_device_that_never_appears_reports_waiting_timeout_then_rejects() {
    let hex = three_block_hex();
    let mut events = Vec::new();
    let err = run_flash_update(
        &hex,
        None,
        fast_options(),
        || {},
        || Err::<MockHalfKayDevice, _>(FlashError::DeviceNotFound),
        |e| events.push(e),
    )
    .unwrap_err();
    assert_eq!(err.kind, FlashFailureKind::NotFound);
    assert_eq!(
        stages(&events),
        ["verified", "waiting_bootloader", "waiting_timeout", "error"]
    );
    assert_eq!(serde_json::to_value(&err).unwrap()["kind"], "not_found");
}

#[test]
fn a_hid_permission_error_is_reported_as_permission() {
    let hex = three_block_hex();
    let mut events = Vec::new();
    let opened = Cell::new(0);
    let err = run_flash_update(
        &hex,
        None,
        fast_options(),
        || {},
        || {
            opened.set(opened.get() + 1);
            Err::<MockHalfKayDevice, _>(classify_hid_open_error(
                "Failed to open a device with path '/dev/hidraw3': Permission denied",
            ))
        },
        |e| events.push(e),
    )
    .unwrap_err();
    assert_eq!(err.kind, FlashFailureKind::Permission);
    // udev may fix the node's permissions a moment after it appears, so it is retried
    // until the wait ends; the device did appear, so this is not a "hasn't switched" timeout.
    assert!(opened.get() > 1);
    assert_eq!(stages(&events), ["verified", "waiting_bootloader", "error"]);
}

#[test]
fn hid_open_errors_are_classified() {
    assert!(matches!(
        classify_hid_open_error("hidapi error: Permission denied"),
        FlashError::PermissionDenied(_)
    ));
    assert!(matches!(
        classify_hid_open_error("Access is denied."),
        FlashError::PermissionDenied(_)
    ));
    assert!(matches!(
        classify_hid_open_error("hidapi error: unable to open device"),
        FlashError::DeviceNotFound
    ));
}

#[test]
fn a_text_file_or_wrong_sha_is_refused_before_any_write() {
    for (text, sha) in [
        ("This is a README, not firmware.\n".to_string(), None),
        (three_block_hex(), Some("00".repeat(32))),
    ] {
        let mut events = Vec::new();
        let opened = Cell::new(false);
        let waited = Cell::new(false);
        let err = run_flash_update(
            &text,
            sha.as_deref(),
            fast_options(),
            || waited.set(true),
            || {
                opened.set(true);
                Err::<MockHalfKayDevice, _>(FlashError::DeviceNotFound)
            },
            |e| events.push(e),
        )
        .unwrap_err();
        assert_eq!(err.kind, FlashFailureKind::BadFile);
        assert!(!opened.get() && !waited.get(), "nothing was touched");
        assert_eq!(stages(&events), ["error"]);
    }
}
