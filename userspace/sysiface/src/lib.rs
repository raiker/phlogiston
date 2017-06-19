#![feature(asm)]
#![no_std]

use core::prelude::v1::*;

pub struct Handle {
	id: u32,
}

pub struct Channel {
	handle: Handle,
}

#[repr(usize)]
include!{"../../../src/syscalls.inc"} //the same enum is used in C++ and Rust

#[cfg(any(target_arch = "arm"))]
unsafe fn syscall(method: SysCallMethod, a: usize, b: usize, c: usize) -> isize {
	let retval: isize;

	asm!("svc #0"
		: "={r0}"(retval)
		: "{r0}"(method as usize), "{r1}"(a), "{r2}"(b), "{r3}"(c)
		:
		: "volatile"
	);

	retval
}

#[deprecated]
pub fn klog_print(msg: &str) {
	unsafe {
		syscall(SysCallMethod::KlogPrint, msg.as_ptr() as usize, msg.len(), 0);
	}
}

pub fn get_time() -> i64 {
	let mut time_val: i64 = 0;
	unsafe {
		syscall(SysCallMethod::GetTime, &mut time_val as *mut i64 as usize, 0, 0);
	}
	time_val
}

pub fn request_channel(name: &str) -> Option<Channel> {
	let handle_id: isize = unsafe {
		syscall(SysCallMethod::RequestChannel, name.as_ptr() as usize, name.len(), 0)
	};

	if handle_id < 0 {
		None
	} else {
		Some(Channel { handle: Handle {id: handle_id as u32}})
	}
}

impl Drop for Handle {
	fn drop(&mut self) {
		unsafe {
			syscall(SysCallMethod::CloseHandle, self.id as usize, 0, 0);
		}
	}
}

#[cfg(test)]
mod tests {
	#[test]
	fn it_works() {
	}
}
