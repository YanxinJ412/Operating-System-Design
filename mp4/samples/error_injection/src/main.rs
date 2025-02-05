#![no_std]
#![no_main]

extern crate rex;


use rex::kprobe::kprobe;
use rex::map::*;
use rex::pt_regs::PtRegs;
use rex::{rex_kprobe, Result};
use rex::{rex_map, bpf_printk};
// create a map to store the errno
#[rex_map]
static MAP_ERRNO: RexHashMap<i32, u32> = RexHashMap::new(1024, 0);
// create a map to store the target pid
#[rex_map]
static MAP_PID: RexHashMap<i32, i32> = RexHashMap::new(1024, 0);
// error injector function
// Input: obj: kprobe object
//        ctx: PtRegs object
// Output: Result
// We will use the kprobe hook point of Rex and the kprobe-override-return mechanism 
// to implement our error injector. Our program should be attached to the entry  of 
// the target system call. As kprobe also allows modifying the contexts it instruments, 
// we can leverage this feature to override the return value and force an early return,
// which involves setting the return value register and the instruction pointer.
#[rex_kprobe]
pub fn err_injector(obj: &kprobe, ctx: &mut PtRegs) -> Result {
    // get the target pid
    let key: i32 = 0;
    let target_pid = match obj.bpf_map_lookup_elem(&MAP_PID, &key) {
        None => {
            bpf_printk!(obj, c"Target Pid Not found.\n");
            return Ok(0);
        }
        Some(val) => *val,
    };
    // get the errno
    let errno = match obj.bpf_map_lookup_elem(&MAP_ERRNO, &key) {
        None => {
            bpf_printk!(obj, c"Error Not found.\n");
            return Ok(0);
        }
        Some(val) => *val,
    };
    // get the current pid
    let current_pid = if let Some(task) = obj.bpf_get_current_task() {
        task.get_pid()
    } else {
        -1
    };
    // Compare the PID of the current process with your target PID 
    // If they do not match, then do an early return
    if current_pid != target_pid{
        bpf_printk!(obj, c"PID mismatch\n");
        return Ok(0);
    };
    // Perform the error injection. 
    // insert the negative errno to the return value register
    let errno_u64 = -(errno as i64) as u64;
    let res = obj.bpf_override_return(ctx, errno_u64);
    if res < 0 {
        return Ok(0);
    }
    return Ok(0);

}
