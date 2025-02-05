// This trait try to implement the READ_ONCE macro in kernel
// no compiletime_assert_rwonce_type
pub(crate) trait ReadOnce<T> {
    fn read_once(var_addr: &T) -> T;
}

impl ReadOnce<u8> for u8 {
    #[inline(always)]
    fn read_once(var_addr: &u8) -> u8 {
        unsafe { core::ptr::read_volatile(var_addr) }
    }
}

impl ReadOnce<u16> for u16 {
    #[inline(always)]
    fn read_once(var_addr: &u16) -> u16 {
        unsafe { core::ptr::read_volatile(var_addr) }
    }
}

impl ReadOnce<u32> for u32 {
    #[inline(always)]
    fn read_once(var_addr: &u32) -> u32 {
        unsafe { core::ptr::read_volatile(var_addr) }
    }
}

impl ReadOnce<u64> for u64 {
    #[inline(always)]
    fn read_once(var_addr: &u64) -> u64 {
        unsafe { core::ptr::read_volatile(var_addr) }
    }
}

impl ReadOnce<i8> for i8 {
    #[inline(always)]
    fn read_once(var_addr: &i8) -> i8 {
        unsafe { core::ptr::read_volatile(var_addr) }
    }
}

impl ReadOnce<i16> for i16 {
    #[inline(always)]
    fn read_once(var_addr: &i16) -> i16 {
        unsafe { core::ptr::read_volatile(var_addr) }
    }
}

impl ReadOnce<i32> for i32 {
    #[inline(always)]
    fn read_once(var_addr: &i32) -> i32 {
        unsafe { core::ptr::read_volatile(var_addr) }
    }
}

impl ReadOnce<i64> for i64 {
    #[inline(always)]
    fn read_once(var_addr: &i64) -> i64 {
        unsafe { core::ptr::read_volatile(var_addr) }
    }
}

impl<T> ReadOnce<*const T> for *const T {
    #[inline(always)]
    fn read_once(var_addr: &*const T) -> *const T {
        unsafe { core::ptr::read_volatile(var_addr) as *const T }
    }
}

impl<T> ReadOnce<*mut T> for *mut T {
    #[inline(always)]
    fn read_once(var_addr: &*mut T) -> *mut T {
        unsafe { core::ptr::read_volatile(var_addr) as *mut T }
    }
}

// follow the coding style with pre_cpu.rs
#[inline(always)]
pub(crate) fn read_once<T: ReadOnce<T>>(var_addr: &T) -> T {
    <T as ReadOnce<T>>::read_once(var_addr)
}
