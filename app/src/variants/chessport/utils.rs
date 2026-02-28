/// Split a string by a delimiter, returning a Vec of string slices.
pub fn split_string<'a>(s: &'a str, delimiter: char) -> Vec<&'a str> {
    s.split(delimiter).collect()
}

/// ASCII lowercase of a character.
#[inline]
pub const fn to_lower(c: u8) -> u8 {
    if c >= b'A' && c <= b'Z' {
        c - b'A' + b'a'
    } else {
        c
    }
}
