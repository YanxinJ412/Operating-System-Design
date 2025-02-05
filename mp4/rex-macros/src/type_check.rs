use proc_macro::TokenStream;
use syn::{Type, TypePath};

fn type_checking(type_path: &TypePath) -> bool {
    let type_name = type_path.path.segments.last().unwrap().ident.to_string();

    match type_name.as_str() {
        "u8" => (),
        "i8" => (),
        "i16" => (),
        "u16" => (),
        "i32" => (),
        "u32" => (),
        "i64" => (),
        "u64" => (),
        _ => return false,
    }

    true
}

pub(crate) fn field_type_check(ty: Type) -> Result<(), TokenStream> {
    if let Type::Path(type_path) = ty {
        if !type_checking(&type_path) {
            return Err(syn::Error::new_spanned(
                &type_path,
                "All fields must be numeric type",
            )
            .to_compile_error()
            .into());
        }
    } else if let Type::Array(type_array) = ty {
        // convert [T; N] to T
        return field_type_check(*type_array.elem);
    } else if let Type::Reference(type_ref) = ty {
        // convert &'a T to T
        return field_type_check(*type_ref.elem);
    } else {
        // TODO: add more detailed type hints
        return Err(syn::Error::new_spanned(
            &ty,
            "All fields must be validate type",
        )
        .to_compile_error()
        .into());
    }
    Ok(())
}
