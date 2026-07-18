use serde::{Deserialize, Serialize};
use std::fs::File;
use std::io::{BufRead, BufReader, Write};
use std::path::Path;

#[derive(Serialize, Deserialize, Debug)]
pub struct ConfigOptions {
    pub lto: bool,
    pub preempt: String,
    pub cpu_opt: String,
    pub zfs_support: bool,
    pub nvidia_support: bool,
}

impl ConfigOptions {
    pub fn new() -> Self {
        ConfigOptions {
            lto: false,
            preempt: "voluntary".to_string(),
            cpu_opt: "generic".to_string(),
            zfs_support: false,
            nvidia_support: false,
        }
    }

    pub fn apply_to_config(&self, config_path: &str) -> std::io::Result<()> {
        let path = Path::new(config_path);
        if !path.exists() {
            return Err(std::io::Error::new(std::io::ErrorKind::NotFound, "Config file not found"));
        }

        let file = File::open(path)?;
        let reader = BufReader::new(file);
        let mut lines: Vec<String> = reader.lines().collect::<Result<_, _>>()?;

        // Simple implementation: replace or append options
        self.update_option(&mut lines, "CONFIG_LTO_CLANG_FULL", if self.lto { "y" } else { "n" });
        
        match self.preempt.as_str() {
            "full" => {
                self.update_option(&mut lines, "CONFIG_PREEMPT", "y");
                self.update_option(&mut lines, "CONFIG_PREEMPT_VOLUNTARY", "n");
            },
            "voluntary" => {
                self.update_option(&mut lines, "CONFIG_PREEMPT", "n");
                self.update_option(&mut lines, "CONFIG_PREEMPT_VOLUNTARY", "y");
            },
            _ => {
                self.update_option(&mut lines, "CONFIG_PREEMPT_NONE", "y");
            }
        }

        let mut out = File::create(path)?;
        for line in lines {
            writeln!(out, "{}", line)?;
        }
        Ok(())
    }

    fn update_option(&self, lines: &mut Vec<String>, key: &str, value: &str) {
        let entry = format!("{}={}", key, value);
        if let Some(pos) = lines.iter().position(|l| l.starts_with(key)) {
            lines[pos] = entry;
        } else {
            lines.push(entry);
        }
    }
}

use std::ffi::CStr;
use std::os::raw::c_char;

#[no_mangle]
pub extern "C" fn apply_config_options(json_ptr: *const c_char, config_path_ptr: *const c_char) -> i32 {
    if json_ptr.is_null() || config_path_ptr.is_null() {
        return -1;
    }

    let json_str = unsafe { CStr::from_ptr(json_ptr) }.to_string_lossy();
    let config_path = unsafe { CStr::from_ptr(config_path_ptr) }.to_string_lossy();

    let options = match serde_json::from_str::<ConfigOptions>(&json_str) {
        Ok(opts) => opts,
        Err(_) => return -2,
    };

    match options.apply_to_config(&config_path) {
        Ok(_) => 0,
        Err(_) => -3,
    }
}

#[no_mangle]
pub extern "C" fn config_options_to_json(lto: bool, preempt_ptr: *const c_char, cpu_opt_ptr: *const c_char, zfs: bool, nvidia: bool) -> *mut c_char {
    if preempt_ptr.is_null() || cpu_opt_ptr.is_null() {
        return std::ptr::null_mut();
    }

    let preempt = unsafe { CStr::from_ptr(preempt_ptr) }.to_string_lossy().to_string();
    let cpu_opt = unsafe { CStr::from_ptr(cpu_opt_ptr) }.to_string_lossy().to_string();

    let options = ConfigOptions {
        lto,
        preempt,
        cpu_opt,
        zfs_support: zfs,
        nvidia_support: nvidia,
    };

    match serde_json::to_string(&options) {
        Ok(json) => {
            let c_str = std::ffi::CString::new(json).unwrap();
            c_str.into_raw()
        }
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn free_c_string(ptr: *mut c_char) {
    if ptr.is_null() {
        return;
    }
    unsafe {
        std::ffi::CString::from_raw(ptr);
    }
}

#[no_mangle]
pub extern "C" fn config_options_from_json(json_ptr: *const c_char, lto: *mut bool, preempt: *mut c_char, cpu_opt: *mut c_char, zfs: *mut bool, nvidia: *mut bool) -> i32 {
    if json_ptr.is_null() || lto.is_null() || preempt.is_null() || cpu_opt.is_null() || zfs.is_null() || nvidia.is_null() {
        return -1;
    }

    let json_str = unsafe { CStr::from_ptr(json_ptr) }.to_string_lossy();

    let options = match serde_json::from_str::<ConfigOptions>(&json_str) {
        Ok(opts) => opts,
        Err(_) => return -2,
    };

    unsafe {
        *lto = options.lto;
        *zfs = options.zfs_support;
        *nvidia = options.nvidia_support;

        let preempt_cstr = std::ffi::CString::new(options.preempt).unwrap();
        std::ptr::copy_nonoverlapping(preempt_cstr.as_ptr(), preempt as *mut c_char, preempt_cstr.as_bytes().len() + 1);

        let cpu_opt_cstr = std::ffi::CString::new(options.cpu_opt).unwrap();
        std::ptr::copy_nonoverlapping(cpu_opt_cstr.as_ptr(), cpu_opt as *mut c_char, cpu_opt_cstr.as_bytes().len() + 1);
    }

    0
}