use proc_macro2::TokenStream;
use quote::{format_ident, quote};
use syn::{parse2, ItemFn, Result};

use crate::args::parse_string_args;

pub(crate) struct KProbe {
    function: Option<String>,
    item: ItemFn,
}

impl KProbe {
    // parse the argument of function
    pub(crate) fn parse(
        attrs: TokenStream,
        item: TokenStream,
    ) -> Result<KProbe> {
        let item: ItemFn = parse2(item)?;
        let args = parse_string_args(attrs)?;

        let function = pop_string_args!(args, "function");

        Ok(KProbe { function, item })
    }

    pub(crate) fn expand(&self) -> Result<TokenStream> {
        let fn_name = self.item.sig.ident.clone();
        let item = &self.item;
        let function_name = format!("{}", fn_name);
        let prog_ident =
            format_ident!("PROG_{}", fn_name.to_string().to_uppercase());

        let attached_function = if self.function.is_some() {
            format!("rex/kprobe/{}", self.function.as_ref().unwrap())
        } else {
            "rex/kprobe".to_string()
        };

        let function_body_tokens = quote! {
            #[inline(always)]
            #item

            #[used]
            #[link_section = #attached_function]
            static #prog_ident: kprobe =
                kprobe::new(#fn_name, #function_name);
        };

        Ok(function_body_tokens)
    }
}
