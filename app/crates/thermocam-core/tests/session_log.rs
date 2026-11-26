use std::fs;
use std::time::{Duration, SystemTime};

use thermocam_core::session_log::{LogSample, SessionLog, LOG_HEADER, MAX_LOG_AGE};

fn sample(t_ms: u64, region_id: &str, value_c: f32) -> LogSample {
    LogSample {
        t_ms,
        region_id: region_id.into(),
        value_c,
    }
}

#[test]
fn three_batches_export_every_sample_in_order() {
    let tmp = tempfile::tempdir().unwrap();
    let dir = tmp.path().join("session");
    let mut log = SessionLog::start(&dir, SystemTime::now()).unwrap();
    assert!(log.path().starts_with(&dir));
    log.append(&[sample(0, "r1", 21.5), sample(0, "r2", 30.25)])
        .unwrap();
    log.append(&[sample(1000, "r1", 21.75)]).unwrap();
    log.append(&[
        sample(2000, "r1", 22.0),
        sample(2000, "r2", 31.0),
        sample(2000, "Spot, \"left\"", 19.5),
    ])
    .unwrap();
    assert_eq!(log.samples(), 6);

    let out = tmp.path().join("export.csv");
    log.export(&out).unwrap();
    let text = fs::read_to_string(&out).unwrap();
    let lines: Vec<&str> = text.lines().collect();
    assert_eq!(lines[0], LOG_HEADER);
    assert_eq!(LOG_HEADER, "t_ms,region_id,value_c");
    assert_eq!(
        &lines[1..],
        &[
            "0,r1,21.50",
            "0,r2,30.25",
            "1000,r1,21.75",
            "2000,r1,22.00",
            "2000,r2,31.00",
            "2000,\"Spot, \"\"left\"\"\",19.50",
        ]
    );
    // Logging continues after an export, and a later export covers everything.
    log.append(&[sample(3000, "r1", 22.5)]).unwrap();
    log.export(&out).unwrap();
    assert_eq!(fs::read_to_string(&out).unwrap().lines().count(), 8);
}

fn age(path: &std::path::Path, by: Duration) {
    fs::File::options()
        .write(true)
        .open(path)
        .unwrap()
        .set_modified(SystemTime::now() - by)
        .unwrap();
}

#[test]
fn logs_older_than_7_days_are_removed_at_start() {
    let tmp = tempfile::tempdir().unwrap();
    let dir = tmp.path().join("session");
    fs::create_dir_all(&dir).unwrap();
    let old = dir.join("1700000000000.csv");
    let recent = dir.join("1760000000000.csv");
    let other = dir.join("notes.txt");
    for p in [&old, &recent, &other] {
        fs::write(p, "t_ms,region_id,value_c\n").unwrap();
    }
    age(&old, Duration::from_secs(8 * 24 * 3600));
    age(&recent, Duration::from_secs(6 * 24 * 3600));
    age(&other, Duration::from_secs(30 * 24 * 3600));
    assert_eq!(MAX_LOG_AGE, Duration::from_secs(7 * 24 * 3600));

    let log = SessionLog::start(&dir, SystemTime::now()).unwrap();
    assert!(!old.exists(), "8-day-old log removed");
    assert!(recent.exists(), "6-day-old log kept");
    assert!(other.exists(), "only session logs are touched");
    assert!(log.path().exists());
}

#[test]
fn two_sessions_started_in_the_same_millisecond_get_separate_files() {
    let tmp = tempfile::tempdir().unwrap();
    let now = SystemTime::now();
    let a = SessionLog::start(tmp.path(), now).unwrap();
    let b = SessionLog::start(tmp.path(), now).unwrap();
    assert_ne!(a.path(), b.path());
}
