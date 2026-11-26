use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};

use thermocam_core::card::{list_card_dir, read_card_file, MAX_READ_CHUNK};
use thermocam_core::dat::{ThermalDat, LEPTON3_BIG_SIZE};
use thermocam_core::protocol::codec::StreamCtrl;
use thermocam_core::protocol::transport::{
    demo_video_frame, MockTransport, ProtocolClient, SimulatorTransport, DEMO_VIDEO_FOLDER,
    DEMO_VIDEO_FRAMES,
};
use thermocam_core::protocol::{MsgType, Packet, HEADER_LEN};

fn big_file() -> Vec<u8> {
    (0..1_048_576u32)
        .map(|i| (i.wrapping_mul(2_654_435_761) >> 24) as u8)
        .collect()
}

#[test]
fn reading_a_1_mb_card_file_keeps_the_stream_flowing() {
    let data = big_file();
    let mut sim = SimulatorTransport::new();
    sim.add_sd_file("big.bin".into(), data.clone());
    let mut client = ProtocolClient::new(sim);
    client.hello().unwrap();
    client
        .set_stream(StreamCtrl {
            mode: 1,
            keep_lcd_on: true,
            include_temp_points: false,
            perf_interval_s: 0,
            max_fps_x10: 0,
        })
        .unwrap();
    let client = Arc::new(Mutex::new(client));

    // The stream loop: take the lock for one bounded poll, record every frame's arrival.
    let stop = Arc::new(AtomicBool::new(false));
    let frames: Arc<Mutex<Vec<Instant>>> = Arc::new(Mutex::new(Vec::new()));
    let stream = {
        let client = Arc::clone(&client);
        let stop = Arc::clone(&stop);
        let frames = Arc::clone(&frames);
        thread::spawn(move || {
            while !stop.load(Ordering::SeqCst) {
                let pkts = client
                    .lock()
                    .unwrap()
                    .poll_packets(Duration::from_millis(10))
                    .unwrap();
                let now = Instant::now();
                let got = pkts
                    .iter()
                    .filter(|p| p.msg_type().ok() == Some(MsgType::FrameRaw))
                    .count();
                if got > 0 {
                    let mut f = frames.lock().unwrap();
                    f.extend(std::iter::repeat_n(now, got));
                }
                thread::yield_now();
            }
        })
    };

    // Let the stream settle, then read the file from another thread.
    thread::sleep(Duration::from_millis(300));
    let before = frames.lock().unwrap().len();
    let started = Instant::now();
    let read = read_card_file(&*client, "/big.bin").unwrap();
    let elapsed = started.elapsed();
    thread::sleep(Duration::from_millis(300));
    stop.store(true, Ordering::SeqCst);
    stream.join().unwrap();

    assert_eq!(read.len(), data.len());
    assert!(read == data, "file content round-trips");

    let times = frames.lock().unwrap().clone();
    assert!(
        times.len() > before + 2,
        "frames kept arriving: {before} -> {}",
        times.len()
    );
    let window_start = started - Duration::from_millis(200);
    let window_end = started + elapsed + Duration::from_millis(200);
    let in_window: Vec<Instant> = times
        .iter()
        .copied()
        .filter(|t| *t >= window_start && *t <= window_end)
        .collect();
    let max_gap = in_window
        .windows(2)
        .map(|w| w[1] - w[0])
        .max()
        .unwrap_or_default();
    assert!(
        max_gap < Duration::from_millis(250),
        "largest gap between frames during the read: {max_gap:?} (read took {elapsed:?})"
    );
}

#[test]
fn listing_the_seeded_video_folder_returns_its_60_frames() {
    let mut client = ProtocolClient::new(SimulatorTransport::new());
    client.hello().unwrap();
    let card = Mutex::new(client);

    let root = list_card_dir(&card, "/").unwrap();
    let folder = root
        .iter()
        .find(|e| e.name == DEMO_VIDEO_FOLDER)
        .expect("seeded video folder in the root");
    assert!(folder.is_dir);
    assert!(root.iter().filter(|e| !e.is_dir).count() >= 3);

    let entries = list_card_dir(&card, &format!("/{DEMO_VIDEO_FOLDER}")).unwrap();
    assert_eq!(entries.len(), DEMO_VIDEO_FRAMES as usize);
    assert_eq!(entries.len(), 60);
    assert_eq!(entries[0].name, "000000.DAT");
    assert_eq!(entries[59].name, "000059.DAT");
    assert!(entries
        .iter()
        .all(|e| !e.is_dir && e.size as usize == LEPTON3_BIG_SIZE));

    let bytes = read_card_file(&card, &format!("/{DEMO_VIDEO_FOLDER}/000059.DAT")).unwrap();
    let dat = ThermalDat::from_bytes(&bytes).unwrap();
    assert_eq!(dat.pixels, demo_video_frame(59).pixels);
}

#[test]
fn simulator_honours_offset_and_length_and_lists_explicit_folders() {
    let mut sim = SimulatorTransport::new();
    sim.add_sd_file("data.bin".into(), (0u8..=255).collect());
    sim.add_sd_dir("EMPTY".into());
    let mut client = ProtocolClient::new(sim);
    client.hello().unwrap();
    let (chunk, eof) = client.read_file_chunk("/data.bin", 10, 5).unwrap();
    assert_eq!(chunk, vec![10, 11, 12, 13, 14]);
    assert!(!eof);
    let (tail, eof) = client.read_file_chunk("/data.bin", 250, 100).unwrap();
    assert_eq!(tail, vec![250, 251, 252, 253, 254, 255]);
    assert!(eof);
    let card = Mutex::new(client);
    let root = list_card_dir(&card, "/").unwrap();
    assert!(root.iter().any(|e| e.name == "EMPTY" && e.is_dir));
    assert!(list_card_dir(&card, "/EMPTY").unwrap().is_empty());
}

#[test]
fn every_read_request_asks_for_a_bounded_length() {
    let mut mock = MockTransport::default();
    let mut c = Vec::new();
    c.extend_from_slice(&0u32.to_le_bytes());
    c.extend_from_slice(&3u16.to_le_bytes());
    c.push(1); // eof
    c.push(0);
    c.extend_from_slice(b"abc");
    mock.push_rx(&Packet::encode(MsgType::FsReadData, 0, 1, 0, &c).unwrap());
    let card = Mutex::new(ProtocolClient::new(mock));
    assert_eq!(read_card_file(&card, "/x.DAT").unwrap(), b"abc");
    let tx = card.into_inner().unwrap().into_inner().tx_log;
    let payload = &tx[HEADER_LEN..];
    let offset = u32::from_le_bytes(payload[0..4].try_into().unwrap());
    let len = u32::from_le_bytes(payload[4..8].try_into().unwrap());
    assert_eq!(offset, 0);
    assert_eq!(len, MAX_READ_CHUNK);
    assert_eq!(MAX_READ_CHUNK, 32_768);
}

#[test]
fn listing_pages_through_large_folders() {
    let mut sim = SimulatorTransport::new();
    for i in 0..150usize {
        sim.add_sd_file(format!("PAGE/{i:06}.DAT"), vec![0u8; 8]);
    }
    let mut client = ProtocolClient::new(sim);
    client.hello().unwrap();
    let card = Mutex::new(client);
    let entries = list_card_dir(&card, "/PAGE").unwrap();
    assert_eq!(entries.len(), 150);
    assert_eq!(entries[149].name, "000149.DAT");
}
