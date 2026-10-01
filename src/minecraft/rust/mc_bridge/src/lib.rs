pub mod coords;

use std::os::raw::c_char;

#[no_mangle]
pub extern "C" fn mc_bridge_version() -> *const c_char {
    b"0.1.0\0".as_ptr() as *const c_char
}
