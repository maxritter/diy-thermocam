//! The live stream's view of the connection: when read errors mean the camera is gone.
//!
//! The app's stream loop feeds every poll result into a [`StreamWatch`]. A few failed reads in
//! a row (an unplugged cable fails every read at once) end the session with one
//! [`CAMERA_DISCONNECTED_EVENT`]; a single hiccup does not.

use std::fmt::Display;

use serde::Serialize;

use super::Packet;

/// The event the app sends when the camera goes away (`{ "reason": … }`).
pub const CAMERA_DISCONNECTED_EVENT: &str = "camera-disconnected";

/// Failed reads in a row that mean the camera is gone.
pub const IO_ERRORS_BEFORE_DISCONNECT: u32 = 5;

/// Payload of [`CAMERA_DISCONNECTED_EVENT`].
#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Disconnected {
    pub reason: String,
}

/// What the stream loop does with one poll.
#[derive(Debug)]
pub enum StreamStep {
    /// Packets (possibly none) to hand on.
    Packets(Vec<Packet>),
    /// A failed read; wait a moment and poll again.
    Retry,
    /// The camera is gone: send the event and end the stream.
    Disconnected(Disconnected),
}

#[derive(Debug, Default)]
pub struct StreamWatch {
    errors: u32,
}

impl StreamWatch {
    /// One poll's result.
    pub fn observe<E: Display>(&mut self, polled: Result<Vec<Packet>, E>) -> StreamStep {
        match polled {
            Ok(pkts) => {
                self.errors = 0;
                StreamStep::Packets(pkts)
            }
            Err(e) => {
                self.errors += 1;
                if self.errors >= IO_ERRORS_BEFORE_DISCONNECT {
                    StreamStep::Disconnected(Disconnected {
                        reason: e.to_string(),
                    })
                } else {
                    StreamStep::Retry
                }
            }
        }
    }
}
