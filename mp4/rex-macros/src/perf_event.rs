use proc_macro2::TokenStream;
use quote::{format_ident, quote};
use syn::{ItemFn, Result};

pub(crate) struct PerfEvent {
    item: ItemFn,
}

impl PerfEvent {
    // parse the argument of function
    pub(crate) fn parse(
        _: TokenStream,
        item: TokenStream,
    ) -> Result<PerfEvent> {
        let item = syn::parse2(item)?;
        Ok(PerfEvent { item })
    }

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
            #[link_section = "rex/perf_event"]
            static #prog_ident: perf_event =
                perf_event::new(#fn_name, #function_name);
        };
        Ok(function_body_tokens)
    }
}
