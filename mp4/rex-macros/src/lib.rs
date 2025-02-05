#[macro_use]
pub(crate) mod args;
mod kprobe;
mod perf_event;
mod tc;
mod tracepoint;
mod type_check;
mod xdp;

use proc_macro::TokenStream;
use proc_macro_error::{abort, proc_macro_error};
use quote::quote;
use std::borrow::Cow;
use syn::{parse_macro_input, Data, DeriveInput, Fields, ItemStatic};

use kprobe::KProbe;
use perf_event::PerfEvent;
use tc::SchedCls;
use tracepoint::TracePoint;
use type_check::field_type_check;
use xdp::Xdp;

#[proc_macro_error]
#[proc_macro_attribute]
pub fn rex_xdp(attrs: TokenStream, item: TokenStream) -> TokenStream {
    match Xdp::parse(attrs.into(), item.into()) {
        Ok(prog) => prog
            .expand()
            .unwrap_or_else(|err| abort!(err.span(), "{}", err))
            .into(),
        Err(err) => abort!(err.span(), "{}", err),
    }
}

#[proc_macro_error]
#[proc_macro_attribute]
pub fn rex_tc(attrs: TokenStream, item: TokenStream) -> TokenStream {
    match SchedCls::parse(attrs.into(), item.into()) {
        Ok(prog) => prog
            .expand()
            .unwrap_or_else(|err| abort!(err.span(), "{}", err))
            .into(),
        Err(err) => abort!(err.span(), "{}", err),
    }
}

#[proc_macro_error]
#[proc_macro_attribute]
pub fn rex_kprobe(attrs: TokenStream, item: TokenStream) -> TokenStream {
    match KProbe::parse(attrs.into(), item.into()) {
        Ok(prog) => prog
            .expand()
            .unwrap_or_else(|err| abort!(err.span(), "{}", err))
            .into(),
        Err(err) => abort!(err.span(), "{}", err),
    }
}

#[proc_macro_error]
#[proc_macro_attribute]
pub fn rex_tracepoint(attrs: TokenStream, item: TokenStream) -> TokenStream {
    match TracePoint::parse(attrs.into(), item.into()) {
        Ok(prog) => prog
            .expand()
            .unwrap_or_else(|err| abort!(err.span(), "{}", err))
            .into(),
        Err(err) => abort!(err.span(), "{}", err),
    }
}

#[proc_macro_error]
#[proc_macro_attribute]
pub fn rex_perf_event(attrs: TokenStream, item: TokenStream) -> TokenStream {
    match PerfEvent::parse(attrs.into(), item.into()) {
        Ok(prog) => prog
            .expand()
            .unwrap_or_else(|err| abort!(err.span(), "{}", err))
            .into(),
        Err(err) => abort!(err.span(), "{}", err),
    }
}

/// Ref: <https://github.com/aya-rs/aya/blob/1cf3d3c222bda0351ee6a2bacf9cee5349556764/aya-ebpf-macros/src/lib.rs#L53>
#[proc_macro_attribute]
pub fn rex_map(_: TokenStream, item: TokenStream) -> TokenStream {
    let item: ItemStatic = syn::parse(item).unwrap();
    let name = item.ident.to_string();
    let section_name: Cow<'_, _> = ".maps".to_string().into();
    (quote! {
        #[link_section = #section_name]
        #[export_name = #name]
        #[allow(non_upper_case_globals)]
        #item
    })
    .into()
}

#[proc_macro_derive(FieldTransmute)]
pub fn ensure_numeric(input: TokenStream) -> TokenStream {
    let input_copy = input.clone();
    let ast: DeriveInput = parse_macro_input!(input as DeriveInput);
    let struct_name = ast.ident;

    if let Data::Struct(s) = ast.data {
        if let Fields::Named(fields) = s.fields {
            for field in fields.named {
                if let Err(err) = field_type_check(field.ty) {
                    return err;
                }
            }
        }
    }

    let ast: DeriveInput = parse_macro_input!(input_copy as DeriveInput);
    let mut fields_token = vec![];

    if let Data::Struct(s) = ast.data {
        for field in s.fields {
            let field_type = &field.ty;
            // get field ident
            let _ = field.ident.as_ref().unwrap();

            let token = quote!( safe_transmute::<#field_type>(); );
            fields_token.push(token);
        }
    }

    // You can still derive other traits, or just generate an empty
    // implementation
    let gen = quote! {
        impl #struct_name {
            #[inline(always)]
            pub(crate) fn from_bytes(data: &mut [u8]) -> &mut #struct_name{
             #(#fields_token)*
             unsafe { convert_slice_to_struct_mut::<#struct_name>(data) }
            }
        }
    };
    gen.into()
}
