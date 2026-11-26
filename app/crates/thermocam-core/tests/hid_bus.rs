//! Regression: listing the real HID bus from many threads at once must not crash the process.
//!
//! Studio polls for a camera in update mode every 2 s from short-lived worker threads. Each
//! call used to create its own `HidApi`; overlapping calls tore down the shared macOS HID
//! manager under each other and the app died with `EXC_BREAKPOINT` in `CFRunLoopAddSource`.
//!
//! This lists devices only (no device is opened), but it needs a real HID stack, so it is
//! ignored by default: run it with `cargo test -p thermocam-core --test hid_bus -- --ignored`.

use std::thread;

use thermocam_core::flasher::{update_mode_present, RealHidBus};

#[test]
#[ignore = "needs a real HID stack; run by hand with --ignored"]
fn listing_the_bus_from_many_threads_at_once_survives() {
    let workers: Vec<_> = (0..16)
        .map(|_| {
            thread::spawn(|| {
                for _ in 0..25 {
                    let _ = update_mode_present(&RealHidBus);
                }
            })
        })
        .collect();
    for w in workers {
        w.join().expect("a listing thread panicked");
    }
}
