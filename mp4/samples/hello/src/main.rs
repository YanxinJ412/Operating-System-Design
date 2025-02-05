#![no_std]
#![no_main]

extern crate rex;

use rex::bpf_printk;
use rex::rex_tracepoint;
use rex::tracepoint::*;
use rex::Result;

#[rex_tracepoint(name = "syscalls/sys_enter_dup", tp_type = "Void")]
fn rex_prog1(obj: &tracepoint, _: tp_ctx) -> Result {
    let option_task = obj.bpf_get_current_task();
    if let Some(task) = option_task {
        let cpu = obj.bpf_get_smp_processor_id();
        let pid = task.get_pid();
        bpf_printk!(
            obj,
            c"Rust triggered from PID %u on CPU %u.\n",
            pid as u64,
            cpu as u64
        );
    }
    Ok(0)
}
