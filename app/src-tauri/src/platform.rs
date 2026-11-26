//! The two operating-system facts Studio needs: the local wall-clock time (capture names) and
//! the free space on the disk that holds a folder (recording limits).

use std::path::Path;

use thermocam_core::captures::LocalStamp;

/// Current local time, or UTC when the system cannot tell its time zone.
#[cfg(unix)]
pub fn local_now() -> LocalStamp {
    // SAFETY: `time` and `localtime_r` only write into the provided out-parameters.
    unsafe {
        let t = libc::time(std::ptr::null_mut());
        let mut tm: libc::tm = std::mem::zeroed();
        if libc::localtime_r(&t, &mut tm).is_null() {
            return utc_now();
        }
        LocalStamp {
            year: (tm.tm_year + 1900) as u16,
            month: (tm.tm_mon + 1) as u8,
            day: tm.tm_mday as u8,
            hour: tm.tm_hour as u8,
            minute: tm.tm_min as u8,
            second: tm.tm_sec.min(59) as u8,
        }
    }
}

#[cfg(windows)]
pub fn local_now() -> LocalStamp {
    use windows_sys::Win32::System::SystemInformation::GetLocalTime;
    // SAFETY: `GetLocalTime` fills the provided SYSTEMTIME.
    let st = unsafe {
        let mut st = std::mem::zeroed();
        GetLocalTime(&mut st);
        st
    };
    LocalStamp {
        year: st.wYear,
        month: st.wMonth as u8,
        day: st.wDay as u8,
        hour: st.wHour as u8,
        minute: st.wMinute as u8,
        second: st.wSecond as u8,
    }
}

#[cfg(not(any(unix, windows)))]
pub fn local_now() -> LocalStamp {
    utc_now()
}

#[allow(dead_code)]
fn utc_now() -> LocalStamp {
    let secs = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map_or(0, |d| d.as_secs());
    let iso = thermocam_core::dat::format_utc_iso8601(secs);
    let n = |a: usize, b: usize| iso[a..b].parse::<u16>().unwrap_or(0);
    LocalStamp {
        year: n(0, 4),
        month: n(5, 7) as u8,
        day: n(8, 10) as u8,
        hour: n(11, 13) as u8,
        minute: n(14, 16) as u8,
        second: n(17, 19) as u8,
    }
}

/// Bytes available to this user on the disk holding `path` (the nearest existing ancestor).
#[cfg(unix)]
pub fn free_bytes(path: &Path) -> Option<u64> {
    use std::os::unix::ffi::OsStrExt;
    let existing = path.ancestors().find(|p| p.exists())?;
    let c = std::ffi::CString::new(existing.as_os_str().as_bytes()).ok()?;
    // SAFETY: `statvfs` writes into the zeroed struct; the C string outlives the call.
    unsafe {
        let mut st: libc::statvfs = std::mem::zeroed();
        if libc::statvfs(c.as_ptr(), &mut st) != 0 {
            return None;
        }
        #[allow(clippy::unnecessary_cast)]
        Some((st.f_bavail as u64).saturating_mul(st.f_frsize as u64))
    }
}

#[cfg(windows)]
pub fn free_bytes(path: &Path) -> Option<u64> {
    use std::os::windows::ffi::OsStrExt;
    use windows_sys::Win32::Storage::FileSystem::GetDiskFreeSpaceExW;
    let existing = path.ancestors().find(|p| p.exists())?;
    let wide: Vec<u16> = existing
        .as_os_str()
        .encode_wide()
        .chain(std::iter::once(0))
        .collect();
    let mut avail: u64 = 0;
    // SAFETY: `wide` is NUL-terminated; the out-pointer is valid for the call.
    let ok = unsafe {
        GetDiskFreeSpaceExW(
            wide.as_ptr(),
            &mut avail,
            std::ptr::null_mut(),
            std::ptr::null_mut(),
        )
    };
    (ok != 0).then_some(avail)
}

#[cfg(not(any(unix, windows)))]
pub fn free_bytes(_path: &Path) -> Option<u64> {
    None
}
