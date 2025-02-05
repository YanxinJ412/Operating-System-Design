use proc_macro2::TokenStream;
use quote::{format_ident, quote};
use syn::{ItemFn, Result};

pub(crate) struct SchedCls {
    item: ItemFn,
}

impl SchedCls {
    // parse the argument of function
    pub(crate) fn parse(_: TokenStream, item: TokenStream) -> Result<SchedCls> {
        let item = syn::parse2(item)?;
        Ok(SchedCls { item })
    }

    // expand the function into two function with original function
    //     #[entry_link(inner_unikernel/tc)]
    // static PROG2: sched_cls = sched_cls::new(
    //     xdp_tx_filter,
    //     "xdp_tx_filter",
    //     BPF_PROG_TYPE_SCHED_CLS as u64,
    // );
    pub(crate) fn expand(&self) -> Result<TokenStream> {
        // TODO: section may update in the future
        let fn_name = self.item.sig.ident.clone();
        let item = &self.item;
        let function_name = format!("{}", fn_name);
        let prog_ident =
            format_ident!("PROG_{}", fn_name.to_string().to_uppercase());

        let function_body_tokens = quote! {
            #[inline(always)]
            #item

            #[used]
            #[link_section = "rex/tc"]
            static #prog_ident: sched_cls =
                sched_cls::new(#fn_name, #function_name,
                BPF_PROG_TYPE_SCHED_CLS as u64);
        };
        Ok(function_body_tokens)
    }
}
