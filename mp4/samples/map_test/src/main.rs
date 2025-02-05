#![no_std]
#![no_main]

extern crate rex;

use rex::linux::bpf::BPF_ANY;
use rex::map::*;
use rex::rex_tracepoint;
use rex::tracepoint::*;
use rex::{bpf_printk, rex_map, Result};

#[rex_map]
static MAP_HASH: RexHashMap<u32, i64> = RexHashMap::new(1024, 0);

#[rex_map]
static MAP_ARRAY: RexArrayMap<u64> = RexArrayMap::new(256, 0);

fn map_test1(obj: &tracepoint) -> Result {
    let key: u32 = 0;

    bpf_printk!(obj, c"Map Testing 1 Start with key %u\n", key as u64);

    match obj.bpf_map_lookup_elem(&MAP_HASH, &key) {
        None => {
            bpf_printk!(obj, c"Not found.\n");
        }
        Some(val) => {
            bpf_printk!(obj, c"Found Val=%llu.\n", (*val) as u64);
        }
    }

    let pid = if let Some(task) = obj.bpf_get_current_task() {
        task.get_pid()
    } else {
        -1
    };
    bpf_printk!(obj, c"Rust program triggered from PID %llu\n", pid as u64);

    obj.bpf_map_update_elem(&MAP_HASH, &key, &(pid as i64), BPF_ANY as u64)?;
    bpf_printk!(obj, c"Map Updated\n");

    match obj.bpf_map_lookup_elem(&MAP_HASH, &key) {
        None => {
            bpf_printk!(obj, c"Not found.\n");
        }
        Some(val) => {
            bpf_printk!(obj, c"Found Val=%llu.\n", (*val) as u64);
        }
    }

    obj.bpf_map_delete_elem(&MAP_HASH, &key)?;
    bpf_printk!(obj, c"Map delete key\n");

    match obj.bpf_map_lookup_elem(&MAP_HASH, &key) {
        None => {
            bpf_printk!(obj, c"Not found.\n");
        }
        Some(val) => {
            bpf_printk!(obj, c"Found Val=%llu.\n", (*val) as u64);
        }
    }

    Ok(0)
}

fn map_test2(obj: &tracepoint) -> Result {
    bpf_printk!(obj, c"Array Map Testing Start\n");
    let key = 0;

    let pid = if let Some(task) = obj.bpf_get_current_task() {
        task.get_pid()
    } else {
        -1
    };
    bpf_printk!(obj, c"Rust program triggered from PID %llu\n", pid as u64);

    // Add a new element
    obj.bpf_map_update_elem(&MAP_ARRAY, &key, &(pid as u64), BPF_ANY as u64)?;
    bpf_printk!(obj, c"Map Updated\n");

    match obj.bpf_map_lookup_elem(&MAP_ARRAY, &key) {
        None => {
            bpf_printk!(obj, c"Not found.\n");
        }
        Some(val) => {
            bpf_printk!(obj, c"Found Val=%llu.\n", *val);
        }
    }
    // let ret = obj.bpf_map_push_elem(MAP_ARRAY, pid as u64, BPF_EXIST.into());
    // bpf_printk!(obj, "Map push ret=%llu\n", ret.try_into().unwrap());

    Ok(0)
}

#[rex_tracepoint(name = "syscalls/sys_enter_dup", tp_type = "Void")]
fn rex_prog1(obj: &tracepoint, _: tp_ctx) -> Result {
    map_test1(obj).map_err(|e| {
        bpf_printk!(obj, c"map_test1 failed with %lld.\n", e as u64);
        e
    })?;
    map_test2(obj).map_err(|e| {
        bpf_printk!(obj, c"map_test2 failed with %lld.\n", e as u64);
        e
    })
}
