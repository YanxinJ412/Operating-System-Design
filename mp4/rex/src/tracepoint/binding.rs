#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct SyscallsEnterOpenArgs {
    pub unused: u64,
    pub syscall_nr: i64,
    pub filename_ptr: i64,
    pub flags: i64,
    pub mode: i64,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct SyscallsExitOpenArgs {
    pub unused: u64,
    pub syscall_nr: i64,
    pub ret: i64,
}
