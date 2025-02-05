use core::ffi::CStr;
use core::ffi::{c_char, c_void};

use crate::base_helper::bpf_trace_printk;
use crate::bindings::linux::kernel::bpf_sysctl_kern;
use crate::bindings::uapi::linux::errno::EINVAL;
use crate::stub;

pub fn str_to_i64(s: &str) -> i64 {
    match s.parse::<i64>() {
        Ok(n) => n,
        Err(e) => {
            bpf_trace_printk("Error parsing string to i64", 0, 0, 0);
            0 // return a default value
        }
    }
}

pub fn str_to_u64(s: &str) -> u64 {
    match s.parse::<u64>() {
        Ok(n) => n,
        Err(e) => {
            bpf_trace_printk("Error parsing string to u64", 0, 0, 0);
            0 // return a default value
        }
    }
}

// TODO check if [u8; N] is a valid type for buf
fn copy_sysctl_value<const N: usize>(
    dst: &mut [u8; N],
    src: *const u8,
    src_len: usize,
) -> i32 {
    let helper: extern "C" fn(*mut u8, usize, *const u8, usize) -> i32 =
        unsafe { core::mem::transmute(stub::copy_sysctl_value_addr()) };
    helper(dst.as_ptr() as *mut u8, dst.len(), src, src_len)
}

pub fn bpf_sysctl_get_current_value<const N: usize>(
    ctx: &bpf_sysctl_kern,
    buf: &mut [u8; N],
) -> i64 {
    // let c_str_ptr: *const c_char = ctx.cur_val as *const c_char;

    // unsafe {
    //     let c_str: &CStr = CStr::from_ptr(c_str_ptr);
    //     let str_slice: &str = c_str.to_str().unwrap();
    // }

    copy_sysctl_value(buf, ctx.cur_val as *const u8, ctx.cur_len) as i64
}

pub fn bpf_sysctl_get_new_value<const N: usize>(
    ctx: &bpf_sysctl_kern,
    buf: &mut [u8; N],
) -> i64 {
    if ctx.write == 0 {
        if !buf.iter().all(|&value| value == 0) {
            // reset if the buffer is not null
            // ERROR doesn't work right now
            // Could we asssume or add check for this?
            buf.fill(0);
        }
        return -(EINVAL as i64);
    }
    return copy_sysctl_value(buf, ctx.new_val as *const u8, ctx.new_len)
        as i64;
}

pub fn bpf_sysctl_set_new_value<const N: usize>(
    ctx: &mut bpf_sysctl_kern,
    buf: &[u8; N],
) -> i64 {
    let helper: extern "C" fn(*mut bpf_sysctl_kern, *const u8, usize) -> i64 =
        unsafe { core::mem::transmute(stub::bpf_sysctl_set_new_value_addr()) };
    helper(ctx as *mut bpf_sysctl_kern, buf.as_ptr() as *const u8, N)
}

pub fn bpf_sysctl_get_name(
    ctx: &mut bpf_sysctl_kern,
    buf: &str,
    flags: u64,
) -> i64 {
    // let mut tmp_ret: isize = 0;
    // let mut ret: isize;

    // if buf.is_null() {
    //     return -libc::EINVAL;
    // }

    // if !(flags & BPF_F_SYSCTL_BASE_NAME) {
    //     if ctx.head.is_null() {
    //         return -EINVAL;
    //     }
    //     tmp_ret = sysctl_cpy_dir(ctx.head.parent, &buf, &buf_len);
    //     if tmp_ret < 0 {
    //         return tmp_ret;
    //     }
    // }

    // ret = strscpy(buf, ctx.table.procname, buf_len);

    // return if ret < 0 { ret } else { tmp_ret + ret };
    let helper: extern "C" fn(
        *mut bpf_sysctl_kern,
        *mut u8,
        usize,
        u64,
    ) -> i64 =
        unsafe { core::mem::transmute(stub::bpf_sysctl_get_name_addr()) };
    helper(
        ctx as *mut bpf_sysctl_kern,
        buf.as_ptr() as *mut u8,
        buf.len(),
        flags,
    )
}
