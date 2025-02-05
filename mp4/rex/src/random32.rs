use crate::bindings::linux::kernel::rnd_state;
use crate::stub;

macro_rules! TAUSWORTHE {
    ($s: expr, $a: expr, $b: expr, $c: expr, $d: expr) => {
        (($s & $c) << $d) ^ ((($s << $a) ^ $s) >> $b)
    };
}

pub(crate) fn prandom_u32_state(state: &mut rnd_state) -> u32 {
    state.s1 = TAUSWORTHE!(state.s1, 6u32, 13u32, 4294967294u32, 18u32);
    state.s2 = TAUSWORTHE!(state.s2, 2u32, 27u32, 4294967288u32, 2u32);
    state.s3 = TAUSWORTHE!(state.s3, 13u32, 21u32, 4294967280u32, 7u32);
    state.s4 = TAUSWORTHE!(state.s4, 3u32, 12u32, 4294967168u32, 13u32);

    state.s1 ^ state.s2 ^ state.s3 ^ state.s4
}

// fn get_cpu_var()
#[inline(always)]
pub(crate) fn bpf_user_rnd_u32() -> u32 {
    // directly use get_random_u32
    unsafe { stub::get_random_u32() }
}
