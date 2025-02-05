pub(crate) trait rex_prog {
    fn prog_run(&self, ctx: *mut ()) -> u32;
}
