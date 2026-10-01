//! C API of the Minecraft bridge (see `mc_bridge.h`). Every entry point checks its pointers,
//! indices and strings and catches panics, so a bad call returns its failure value.
pub mod catalog;
pub mod coords;
pub mod names;
pub mod survival;
pub mod world;

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_void};
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::path::Path;
use std::ptr::null_mut;
use std::sync::Mutex;
use survival::{names_from_table, McMineResult, McStack, McSurvival};
use world::{GetFn, SetFn};

/// Upper bound for `item_count`, far above the C++ table, against garbage counts.
const MAX_ITEMS: i32 = 4096;

static LAST_ERROR: Mutex<Option<CString>> = Mutex::new(None);

fn guard<T>(default: T, f: impl FnOnce() -> T) -> T {
    catch_unwind(AssertUnwindSafe(f)).unwrap_or(default)
}

fn set_last_error(message: String) {
    let message = CString::new(message.replace('\0', " ")).unwrap_or_default();
    *LAST_ERROR.lock().unwrap_or_else(|e| e.into_inner()) = Some(message);
}

/// # Safety
/// `p` is null or a NUL-terminated string that outlives `'a`.
unsafe fn c_str<'a>(p: *const c_char) -> Option<&'a str> {
    if p.is_null() {
        return None;
    }
    CStr::from_ptr(p).to_str().ok()
}

#[no_mangle]
pub extern "C" fn mc_bridge_version() -> *const c_char {
    c"0.1.0".as_ptr()
}

/// # Safety
/// Strings are null or NUL-terminated; `item_names` is null or holds `item_count` such pointers.
#[no_mangle]
pub unsafe extern "C" fn mc_survival_create(
    client_jar: *const c_char,
    item_catalog_json: *const c_char,
    item_names: *const *const c_char,
    item_count: i32,
) -> *mut McSurvival {
    let created = catch_unwind(AssertUnwindSafe(|| -> Result<Box<McSurvival>, String> {
        let jar = c_str(client_jar).ok_or("client_jar is null or not UTF-8")?;
        let catalog = c_str(item_catalog_json).ok_or("item_catalog_json is null or not UTF-8")?;
        if item_names.is_null() {
            return Err("item_names is null".into());
        }
        if !(1..=MAX_ITEMS).contains(&item_count) {
            return Err(format!("item_count {item_count} out of range"));
        }
        let names = std::slice::from_raw_parts(item_names, item_count as usize)
            .iter()
            .enumerate()
            .map(|(i, &name)| {
                c_str(name)
                    .map(str::to_owned)
                    .ok_or_else(|| format!("item name {i} is null or not UTF-8"))
            })
            .collect::<Result<Vec<_>, _>>()?;
        McSurvival::new(Path::new(jar), Path::new(catalog), names_from_table(names))
            .map(Box::new)
            .map_err(|e| format!("{e:#}"))
    }));
    match created {
        Ok(Ok(survival)) => Box::into_raw(survival),
        Ok(Err(message)) => {
            set_last_error(message);
            null_mut()
        }
        Err(_) => {
            set_last_error("panic while loading the survival data".into());
            null_mut()
        }
    }
}

/// # Safety
/// `s` is null or a pointer from `mc_survival_create` not yet destroyed.
#[no_mangle]
pub unsafe extern "C" fn mc_survival_destroy(s: *mut McSurvival) {
    guard((), || {
        if !s.is_null() {
            drop(Box::from_raw(s));
        }
    })
}

/// The reason the last `mc_survival_create` failed ("" if none). Valid until the next failure.
#[no_mangle]
pub extern "C" fn mc_survival_last_error() -> *const c_char {
    let empty = c"".as_ptr();
    guard(empty, || {
        let last = LAST_ERROR.lock().unwrap_or_else(|e| e.into_inner());
        last.as_ref().map_or(empty, |message| message.as_ptr())
    })
}

/// # Safety
/// `s` is null or live; `out` is null or writable.
#[no_mangle]
pub unsafe extern "C" fn mc_inv_get(
    s: *mut McSurvival,
    area: i32,
    index: i32,
    out: *mut McStack,
) -> i32 {
    guard(0, || {
        let Some(out) = out.as_mut() else { return 0 };
        *out = McStack::default();
        let Some(s) = s.as_ref() else { return 0 };
        match s.get(area, index) {
            Some(stack) => {
                *out = stack;
                1
            }
            None => 0,
        }
    })
}

/// # Safety
/// `s` is null or live.
#[no_mangle]
pub unsafe extern "C" fn mc_inv_click(
    s: *mut McSurvival,
    area: i32,
    index: i32,
    right: i32,
    shift: i32,
) -> i32 {
    guard(0, || {
        let Some(s) = s.as_mut() else { return 0 };
        s.click(area, index, right != 0, shift != 0) as i32
    })
}

/// # Safety
/// `s` is null or live.
#[no_mangle]
pub unsafe extern "C" fn mc_inv_take_output(s: *mut McSurvival, workbench: i32, shift: i32) -> i32 {
    guard(0, || {
        let Some(s) = s.as_mut() else { return 0 };
        s.take_output(workbench != 0, shift != 0) as i32
    })
}

/// # Safety
/// `s` is null or live.
#[no_mangle]
pub unsafe extern "C" fn mc_inv_close(s: *mut McSurvival) -> i32 {
    guard(-1, || {
        let Some(s) = s.as_mut() else { return -1 };
        s.close()
    })
}

/// # Safety
/// `s` is null or live.
#[no_mangle]
pub unsafe extern "C" fn mc_inv_add(s: *mut McSurvival, item: i32, count: i32) -> i32 {
    guard(-1, || {
        let Some(s) = s.as_mut() else { return -1 };
        s.add(item, count).unwrap_or(-1)
    })
}

/// # Safety
/// `s` is null or live.
#[no_mangle]
pub unsafe extern "C" fn mc_inv_consume(s: *mut McSurvival, slot: i32, count: i32) -> i32 {
    guard(0, || {
        let Some(s) = s.as_mut() else { return 0 };
        s.consume(slot, count) as i32
    })
}

/// # Safety
/// `s` is null or live; `eye`/`dir` are null or point to 3 doubles; `out` is null or writable;
/// `get`/`set` are null (the call returns 0) or safe to call with `ctx` during this call.
#[no_mangle]
pub unsafe extern "C" fn mc_survival_mine(
    s: *mut McSurvival,
    get: Option<GetFn>,
    set: Option<SetFn>,
    ctx: *mut c_void,
    eye: *const f64,
    dir: *const f64,
    attacking: i32,
    on_ground: i32,
    selected_slot: i32,
    out: *mut McMineResult,
) -> i32 {
    guard(0, || {
        let Some(out) = out.as_mut() else { return 0 };
        *out = McMineResult::default();
        let Some(s) = s.as_mut() else { return 0 };
        let (Some(get), Some(set)) = (get, set) else {
            return 0;
        };
        if eye.is_null() || dir.is_null() {
            return 0;
        }
        let eye = *(eye as *const [f64; 3]);
        let dir = *(dir as *const [f64; 3]);
        match s.mine(
            get,
            set,
            ctx,
            eye,
            dir,
            attacking != 0,
            on_ground != 0,
            selected_slot,
        ) {
            Some(result) => {
                *out = result;
                1
            }
            None => 0,
        }
    })
}

/// # Safety
/// `s` is null or live.
#[no_mangle]
pub unsafe extern "C" fn mc_survival_stop_mining(s: *mut McSurvival) {
    guard((), || {
        if let Some(s) = s.as_mut() {
            s.mining.reset();
        }
    })
}

/// # Safety
/// `s` is null or live; `path` is null or NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn mc_survival_save(s: *mut McSurvival, path: *const c_char) -> i32 {
    guard(0, || {
        let (Some(s), Some(path)) = (s.as_ref(), c_str(path)) else {
            return 0;
        };
        s.save(Path::new(path)) as i32
    })
}

/// # Safety
/// `s` is null or live; `path` is null or NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn mc_survival_load(s: *mut McSurvival, path: *const c_char) -> i32 {
    guard(0, || {
        let (Some(s), Some(path)) = (s.as_mut(), c_str(path)) else {
            return 0;
        };
        s.load(Path::new(path)) as i32
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::survival::tests::{names, real_data, ScratchDir};
    use std::ffi::{CStr, CString};
    use std::os::unix::ffi::OsStrExt;
    use std::ptr::{null, null_mut};

    unsafe extern "C" fn no_get(_: *mut c_void, _: i32, _: i32, _: i32) -> i32 {
        0
    }
    unsafe extern "C" fn no_set(_: *mut c_void, _: i32, _: i32, _: i32, _: i32) {}

    fn bare() -> *mut McSurvival {
        let table = names(&["minecraft:air", "minecraft:dirt", "minecraft:stick"], 1);
        Box::into_raw(Box::new(McSurvival::bare(table)))
    }

    fn last_error() -> String {
        unsafe { CStr::from_ptr(mc_survival_last_error()) }
            .to_string_lossy()
            .into_owned()
    }

    #[test]
    fn null_survival_fails_everywhere() {
        let s = null_mut();
        let mut stack = McStack::default();
        let mut result = McMineResult::default();
        let path = CString::new("/nonexistent/inventory.json").unwrap();
        let v = [0.0; 3];
        unsafe {
            assert_eq!(mc_inv_get(s, 0, 0, &mut stack), 0);
            assert_eq!(mc_inv_click(s, 0, 0, 0, 0), 0);
            assert_eq!(mc_inv_take_output(s, 0, 0), 0);
            assert_eq!(mc_inv_close(s), -1);
            assert_eq!(mc_inv_add(s, 1, 1), -1);
            assert_eq!(mc_inv_consume(s, 0, 1), 0);
            let mined = mc_survival_mine(
                s,
                Some(no_get),
                Some(no_set),
                null_mut(),
                v.as_ptr(),
                v.as_ptr(),
                1,
                1,
                0,
                &mut result,
            );
            assert_eq!(mined, 0);
            mc_survival_stop_mining(s);
            assert_eq!(mc_survival_save(s, path.as_ptr()), 0);
            assert_eq!(mc_survival_load(s, path.as_ptr()), 0);
            mc_survival_destroy(s);
        }
    }

    #[test]
    fn bad_arguments_fail_without_panicking() {
        let s = bare();
        let mut stack = McStack {
            item: 9,
            count: 9,
            damage: 9,
            max_damage: 9,
        };
        let mut result = McMineResult::default();
        let v = [0.0, 1.0, 0.0];
        unsafe {
            assert_eq!(mc_inv_add(s, 1, 3), 0);
            assert_eq!(mc_inv_get(s, 0, 0, null_mut()), 0);
            for (area, index) in [(-1, 0), (6, 0), (0, -1), (0, 40), (1, 4), (2, 9), (3, 1)] {
                assert_eq!(mc_inv_get(s, area, index, &mut stack), 0);
                assert_eq!(stack, McStack::default());
                assert_eq!(mc_inv_click(s, area, index, 0, 0), 0);
            }
            assert_eq!(mc_inv_add(s, 0, 1), -1);
            assert_eq!(mc_inv_add(s, 3, 1), -1);
            assert_eq!(mc_inv_add(s, 1, -4), -1);
            assert_eq!(mc_inv_consume(s, 41, 1), 0);
            assert_eq!(mc_inv_consume(s, 0, -1), 0);
            for selected in [-1, 9] {
                let mined = mc_survival_mine(
                    s,
                    Some(no_get),
                    Some(no_set),
                    null_mut(),
                    v.as_ptr(),
                    v.as_ptr(),
                    1,
                    1,
                    selected,
                    &mut result,
                );
                assert_eq!(mined, 0);
            }
            assert_eq!(
                mc_survival_mine(
                    s,
                    Some(no_get),
                    Some(no_set),
                    null_mut(),
                    null(),
                    v.as_ptr(),
                    1,
                    1,
                    0,
                    &mut result
                ),
                0
            );
            assert_eq!(
                mc_survival_mine(
                    s,
                    Some(no_get),
                    Some(no_set),
                    null_mut(),
                    v.as_ptr(),
                    v.as_ptr(),
                    1,
                    1,
                    0,
                    null_mut()
                ),
                0
            );
            let north = [0.0, 1.0, 0.0];
            let eye = [0.5, 0.5, 1.5];
            let callbacks: [(Option<GetFn>, Option<SetFn>); 3] =
                [(None, Some(no_set)), (Some(no_get), None), (None, None)];
            for (get, set) in callbacks {
                result.broken = 7;
                let mined = mc_survival_mine(
                    s,
                    get,
                    set,
                    null_mut(),
                    eye.as_ptr(),
                    north.as_ptr(),
                    1,
                    1,
                    0,
                    &mut result,
                );
                assert_eq!(mined, 0);
                assert_eq!(result, McMineResult::default());
            }
            assert_eq!(mc_survival_save(s, null()), 0);
            assert_eq!(mc_survival_load(s, null()), 0);
            let bad_utf8 = [0xffu8, 0xfe, 0];
            assert_eq!(mc_survival_save(s, bad_utf8.as_ptr() as *const c_char), 0);
            mc_survival_destroy(s);
        }
    }

    #[test]
    fn calls_reach_the_survival_state() {
        let s = bare();
        let mut stack = McStack::default();
        let mut result = McMineResult::default();
        let eye = [0.5, 0.5, 1.5];
        let north = [0.0, 1.0, 0.0];
        unsafe {
            assert_eq!(mc_inv_add(s, 2, 5), 0);
            assert_eq!(mc_inv_get(s, 0, 0, &mut stack), 1);
            assert_eq!((stack.item, stack.count), (2, 5));
            assert_eq!(mc_inv_consume(s, 0, 2), 1);
            assert_eq!(mc_inv_click(s, 0, 0, 0, 0), 1);
            assert_eq!(mc_inv_get(s, 3, 0, &mut stack), 1);
            assert_eq!((stack.item, stack.count), (2, 3));
            assert_eq!(mc_inv_click(s, 1, 2, 1, 0), 1);
            assert_eq!(mc_inv_get(s, 1, 2, &mut stack), 1);
            assert_eq!(mc_inv_take_output(s, 0, 0), 0);
            assert_eq!(mc_inv_close(s), 0);
            assert_eq!((*s).inventory.count("minecraft:stick"), 3);
            let mined = mc_survival_mine(
                s,
                Some(no_get),
                Some(no_set),
                null_mut(),
                eye.as_ptr(),
                north.as_ptr(),
                1,
                1,
                0,
                &mut result,
            );
            assert_eq!((mined, result.broken), (1, 0));
            for odd in [f64::NAN, f64::INFINITY, -1e300, 2147483645.0, -2147483645.0] {
                let far = [odd; 3];
                let mined = mc_survival_mine(
                    s,
                    Some(no_get),
                    Some(no_set),
                    null_mut(),
                    far.as_ptr(),
                    far.as_ptr(),
                    1,
                    1,
                    0,
                    &mut result,
                );
                assert!(mined == 0 || result.broken == 0, "{odd}");
            }
            mc_survival_stop_mining(s);
            mc_survival_destroy(s);
        }
    }

    #[test]
    fn save_and_load_through_c_paths() {
        let s = bare();
        let dir = ScratchDir::new("ffi_save_load");
        let path = CString::new(dir.file("inv.json").as_os_str().as_bytes()).unwrap();
        let missing = CString::new(dir.file("missing.json").as_os_str().as_bytes()).unwrap();
        unsafe {
            mc_inv_add(s, 1, 7);
            assert_eq!(mc_survival_save(s, path.as_ptr()), 1);
            assert_eq!(mc_survival_load(s, missing.as_ptr()), 0);
            assert!((*s).inventory.slots.iter().all(Option::is_none));
            assert_eq!(mc_survival_load(s, path.as_ptr()), 1);
            assert_eq!((*s).inventory.count("minecraft:dirt"), 7);
            mc_survival_destroy(s);
        }
    }

    #[test]
    fn create_reports_why_it_failed() {
        let air = CString::new("minecraft:air").unwrap();
        let names = [air.as_ptr()];
        let nowhere = CString::new("/nonexistent/client.jar").unwrap();
        unsafe {
            let s = mc_survival_create(null(), nowhere.as_ptr(), names.as_ptr(), 1);
            assert!(s.is_null());
            assert!(last_error().contains("client_jar"));

            let with_null = [air.as_ptr(), null()];
            let s = mc_survival_create(nowhere.as_ptr(), nowhere.as_ptr(), with_null.as_ptr(), 2);
            assert!(s.is_null());
            assert!(last_error().contains("item name 1"));

            let s = mc_survival_create(nowhere.as_ptr(), nowhere.as_ptr(), null(), 1);
            assert!(s.is_null());
            let s = mc_survival_create(nowhere.as_ptr(), nowhere.as_ptr(), names.as_ptr(), 0);
            assert!(s.is_null());
            assert!(last_error().contains("item_count"));

            let s = mc_survival_create(nowhere.as_ptr(), nowhere.as_ptr(), names.as_ptr(), 1);
            assert!(s.is_null());
            assert!(last_error().contains("/nonexistent"), "{}", last_error());
        }
    }

    #[test]
    fn create_with_the_real_data() {
        let Some((jar, catalog)) = real_data("create_with_the_real_data") else {
            return;
        };
        let jar = CString::new(jar.as_os_str().as_bytes()).unwrap();
        let catalog = CString::new(catalog.as_os_str().as_bytes()).unwrap();
        let table: Vec<CString> = [
            "minecraft:air",
            "minecraft:dirt",
            "minecraft:obsidian",
            "minecraft:stick",
        ]
        .iter()
        .map(|n| CString::new(*n).unwrap())
        .collect();
        let ptrs: Vec<*const c_char> = table.iter().map(|n| n.as_ptr()).collect();
        unsafe {
            let s = mc_survival_create(jar.as_ptr(), catalog.as_ptr(), ptrs.as_ptr(), 4);
            assert!(!s.is_null(), "{}", last_error());
            assert!((*s).names.is_block(2));
            assert!(!(*s).names.is_block(3));
            mc_survival_destroy(s);
        }
    }
}
