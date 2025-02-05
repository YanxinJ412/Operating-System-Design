use crate::task_struct::TaskStruct;
use core::arch::asm;
use core::sync::atomic::{compiler_fence, Ordering};

// Compiler hint for unlikely/likely for if branches
#[cold]
#[inline(always)]
fn cold() {}

#[inline(always)]
pub(crate) fn likely(b: bool) -> bool {
    if !b {
        cold()
    }
    b
}

#[inline(always)]
pub(crate) fn unlikely(b: bool) -> bool {
    if b {
        cold()
    }
    b
}

// FIX: CONFIG_KCSAN is not defined
// use crate::bindings::linux::kernel::kcsan_ctx;
// #[inline(always)]
// pub(crate) fn get_ctx() -> &'static kcsan_ctx {
//     // TODO: add check for interrupt context
//     TaskStruct::get_current_task().unwrap().kptr.kcsan_ctx
// }

#[inline(always)]
pub(crate) fn kcsan_atomic_next(_: u32) {}

/// Compiler barrier to prevent instruction reordering across the barrier.
#[inline(always)]
pub(crate) fn barrier() {
    compiler_fence(Ordering::SeqCst);
    // unsafe {
    //     asm!("", options(nomem, nostack));
    // }
}

#[macro_export]
macro_rules! preempt_disable {
    () => {
        barrier();
    };
}

pub(crate) use preempt_disable;
