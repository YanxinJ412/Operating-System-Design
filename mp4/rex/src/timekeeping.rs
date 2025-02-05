use crate::bindings::linux::kernel::CONFIG_CLOCKSOURCE_VALIDATE_LAST_CYCLE;
use crate::bindings::linux::kernel::{
    clocksource, seqcount_latch_t, seqcount_raw_spinlock_t, timekeeper,
    timespec64, tk_read_base,
};
use crate::debug::printk;
use crate::read_once::read_once;
use crate::seqlock::{raw_read_seqcount_latch, read_seqcount_latch_retry};
use crate::stub;

pub(crate) type ktime_t = i64;
pub(crate) type time64_t = i64;
const NSEC_PER_SEC: i64 = 1000000000; // long in amd64 is 8 bytes
const KTIME_MAX: i64 = !(1u64 << 63) as i64;
const KTIME_SEC_MAX: i64 = (KTIME_MAX / NSEC_PER_SEC);

// This struct is an instance of an unnamed struct in kernel timekeeping.c so
// have to redefine it here
#[repr(C)]
struct TKCore {
    seq: seqcount_raw_spinlock_t,
    timekeeper: timekeeper,
}

// This struct is defined in include/linux/timekeeper.c so have to redefine it
// here
#[repr(C)]
struct TimeKeeperFast {
    seq: seqcount_latch_t,
    base: [tk_read_base; 2],
}

#[inline(always)]
fn ktime_set(secs: ktime_t, nsecs: u64) -> ktime_t {
    if secs >= KTIME_SEC_MAX {
        KTIME_MAX
    } else {
        secs * NSEC_PER_SEC + nsecs as ktime_t
    }
}

#[inline(always)]
fn timespec64_to_ktime(ts: timespec64) -> ktime_t {
    ktime_set(ts.tv_sec, ts.tv_nsec as u64)
}

#[inline(always)]
pub(crate) fn clocksource_delta(now: u64, last: u64, mask: u64) -> u64 {
    let ret: u64 = (now - last) & mask;

    if CONFIG_CLOCKSOURCE_VALIDATE_LAST_CYCLE != 0 {
        if ret & !(mask >> 1) != 0 {
            0
        } else {
            ret
        }
    } else {
        (now - last) & mask
    }
}

#[inline(always)]
fn timekeeping_delta_to_ns(tkr: &tk_read_base, delta: u64) -> u64 {
    let mut nsec: u64 = 0;

    nsec = delta * tkr.mult as u64 + tkr.xtime_nsec;
    nsec >>= tkr.shift;

    nsec
}

#[inline(always)]
fn tk_clock_read(tkr: &tk_read_base) -> u64 {
    let clock: &mut clocksource = unsafe { &mut *read_once(&tkr.clock) };

    let read_fn: extern "C" fn(*mut clocksource) -> u64 =
        unsafe { core::mem::transmute(clock.read) };

    read_fn(clock as *mut clocksource)
}

#[inline(always)]
fn ktime_get_fast_ns(tkf: &TimeKeeperFast) -> u64 {
    let mut tkr: &tk_read_base;
    let mut seq: u32 = 0;
    let mut now: u64 = 0;

    loop {
        seq = raw_read_seqcount_latch(&tkf.seq);
        tkr = &tkf.base[(seq & 0x01) as usize];
        now = tkr.base as u64;
        now += timekeeping_delta_to_ns(
            tkr,
            clocksource_delta(tk_clock_read(tkr), tkr.cycle_last, tkr.mask),
        );
        if !read_seqcount_latch_retry(&tkf.seq, seq) {
            break;
        }
    }

    now
}

#[inline(always)]
pub(crate) fn ktime_get_mono_fast_ns() -> u64 {
    let tk_fast: &TimeKeeperFast =
        unsafe { &*(stub::tk_fast_mono_addr() as *mut TimeKeeperFast) };

    ktime_get_fast_ns(tk_fast)
}

#[inline(always)]
pub(crate) fn ktime_get_boot_fast_ns() -> u64 {
    let tk_core = unsafe { &*(stub::tk_core_addr() as *mut TKCore) };
    let tk: &timekeeper = &tk_core.timekeeper;

    (ktime_get_mono_fast_ns() as i64 + tk.offs_boot) as u64
}

#[inline(always)]
pub(crate) fn ktime_get_coarse() -> ktime_t {
    let mut ts: timespec64 = timespec64 {
        tv_sec: 0,
        tv_nsec: 0,
    };
    let ktime_get_coarse_ts64: extern "C" fn(*mut timespec64) -> () =
        unsafe { core::mem::transmute(stub::ktime_get_coarse_ts64_addr()) };

    ktime_get_coarse_ts64(&mut ts);
    timespec64_to_ktime(ts)
}
