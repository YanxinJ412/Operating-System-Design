#![no_std]
#![no_main]
#![allow(non_upper_case_globals)]

extern crate rex;

use rex::bpf_printk;
use rex::kprobe::*;
use rex::linux::seccomp::seccomp_data;
use rex::linux::unistd::*;
use rex::pt_regs::PtRegs;
use rex::rex_kprobe;
use rex::Result;

pub fn func_sys_write(obj: &kprobe, ctx: &PtRegs) -> Result {
    let mut sd: seccomp_data = seccomp_data {
        nr: 0,
        arch: 0,
        instruction_pointer: 0,
        args: [0; 6],
    };

    let unsafe_ptr = ctx.rsi() as *const ();
    obj.bpf_probe_read_kernel(&mut sd, unsafe_ptr)?;

    if sd.args[2] == 512 {
        bpf_printk!(
            obj,
            c"write(fd=%d, buf=%p, size=%d)\n",
            sd.args[0],
            sd.args[1],
            sd.args[2]
        );
    }
    Ok(0)
}

pub fn func_sys_read(obj: &kprobe, ctx: &PtRegs) -> Result {
    let mut sd: seccomp_data = seccomp_data {
        nr: 0,
        arch: 0,
        instruction_pointer: 0,
        args: [0; 6],
    };

    let unsafe_ptr = ctx.rsi() as *const ();
    obj.bpf_probe_read_kernel(&mut sd, unsafe_ptr)?;

    if sd.args[2] > 128 && sd.args[2] <= 1024 {
        bpf_printk!(
            obj,
            c"read(fd=%d, buf=%p, size=%d)\n",
            sd.args[0],
            sd.args[1],
            sd.args[2]
        );
    }
    Ok(0)
}

pub fn func_sys_mmap(obj: &kprobe, _: &PtRegs) -> Result {
    bpf_printk!(obj, c"mmap\n");
    Ok(0)
}

#[rex_kprobe(function = "__seccomp_filter")]
fn rex_prog1(obj: &kprobe, ctx: &mut PtRegs) -> Result {
    match ctx.rdi() as u32 {
        __NR_read => func_sys_read(obj, ctx),
        __NR_write => func_sys_write(obj, ctx),
        __NR_mmap => func_sys_mmap(obj, ctx),
        __NR_getuid..=__NR_getsid => {
            bpf_printk!(
                obj,
                c"syscall=%d (one of get/set uid/pid/gid)\n",
                ctx.rdi()
            );
            Ok(0)
        }
        _ => Ok(0),
    }
}
