use crate::bindings::linux::kernel::{
    seqcount_latch_t, seqcount_raw_spinlock_t, seqcount_t,
};

use crate::barrier::*;
use crate::read_once::read_once;

#[inline(always)]
fn __seqprop_sequence(s: &seqcount_t) -> u32 {
    read_once(&s.sequence)
}

#[inline(always)]
fn __seqprop_preemptible(s: &seqcount_t) -> bool {
    false
}

#[inline(always)]
fn __seqprop_assert(s: &seqcount_t) {
    // lockdep_assert_preemption_disabled();
}

pub(crate) enum PropTy {
    Ptr,
    Sequence,
    Preemptible,
    Assert,
}

// TODO: implement different lock type
pub(crate) trait SeqProp<'a, T> {
    fn seq_prop(s: &'a seqcount_t, prop: PropTy) -> T;
}

impl<'a> SeqProp<'a, &'a seqcount_t> for seqcount_t {
    fn seq_prop(s: &'a seqcount_t, prop: PropTy) -> &'a seqcount_t {
        match prop {
            PropTy::Ptr => s,
            _ => panic!("not implemented"),
        }
    }
}

impl SeqProp<'_, u32> for u32 {
    fn seq_prop(s: &seqcount_t, prop: PropTy) -> u32 {
        match prop {
            PropTy::Sequence => __seqprop_sequence(s),
            _ => panic!("not implemented"),
        }
    }
}

impl SeqProp<'_, bool> for bool {
    fn seq_prop(s: &seqcount_t, prop: PropTy) -> bool {
        match prop {
            PropTy::Preemptible => __seqprop_preemptible(s),
            _ => panic!("not implemented"),
        }
    }
}

// TODO: not implemented yet
impl SeqProp<'_, ()> for () {
    fn seq_prop(s: &seqcount_t, prop: PropTy) {
        match prop {
            PropTy::Assert => __seqprop_assert(s),
            _ => panic!("not implemented"),
        }
    }
}

// follow the coding style with pre_cpu.rs
#[inline(always)]
pub(crate) fn seq_prop<'a, T: SeqProp<'a, T>>(
    s: &'a seqcount_t,
    prop: PropTy,
) -> T {
    <T as SeqProp<T>>::seq_prop(s, prop)
}

#[inline(always)]
pub(crate) fn do_read_seqcount_retry_wrap(s: &seqcount_t, start: u32) -> bool {
    kcsan_atomic_next(0); // TODO not implement yet since config doesn't enable kcsan
    unlikely(read_once(&s.sequence) != start)
}

#[inline(always)]
pub(crate) fn do_read_seqcount_retry(s: &seqcount_t, start: u32) -> bool {
    barrier();
    do_read_seqcount_retry_wrap(s, start)
}

/// read_seqcount_retry() - end a seqcount_t read critical section
/// @s: Pointer to seqcount_t or any of the seqcount_LOCKNAME_t variants
/// @start: count, from read_seqcount_begin()

/// read_seqcount_retry closes the read critical section of given
/// seqcount_t.  If the critical section was invalid, it must be ignored
/// (and typically retried).

/// Return: true if a read section retry is required, else false
#[inline(always)]
pub(crate) fn read_seqcount_retry(s: &seqcount_t, start: u32) -> bool {
    do_read_seqcount_retry(<seqcount_t>::seq_prop(s, PropTy::Ptr), start)
}

#[inline(always)]
pub(crate) fn raw_read_seqcount_latch(seq: &seqcount_latch_t) -> u32 {
    read_once(&seq.seqcount.sequence)
}

#[inline(always)]
pub(crate) fn read_seqcount_latch_retry(
    s: &seqcount_latch_t,
    start: u32,
) -> bool {
    read_seqcount_retry(&s.seqcount, start)
}
