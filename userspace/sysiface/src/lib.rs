#![feature(asm)]
#![no_std]

use core::prelude::v1::*;

#[macro_use]
extern crate bitflags;

#[repr(usize)]
include!{"../../../src/syscalls.inc"} //the same enum is used in C++ and Rust

#[cfg(any(target_arch = "arm"))]
unsafe fn syscall(method: SysCallMethod, a: usize, b: usize, c: usize) -> (usize, usize) {
	let r0: usize;
	let r1: usize;

	asm!("svc #0"
		: "={r0}"(r0), "={r1}"(r1)
		: "{r3}"(method as usize), "{r0}"(a), "{r1}"(b), "{r2}"(c)
		:
		: "volatile"
	);

	(r0, r1)
}

pub struct Time(i64);

pub struct Handle {
	id: u32,
}

pub struct Channel {
	handle: Handle,
}

pub struct ChannelListener {
	handle: Handle,
}

pub struct PollSet {
	handle: Handle,
}

#[deprecated]
pub fn klog_print(msg: &str) {
	unsafe {
		syscall(SysCallMethod::KlogPrint, msg.as_ptr() as usize, msg.len(), 0);
	}
}

pub fn get_time() -> Time {
	let (r0, r1) = unsafe {
		syscall(SysCallMethod::GetTime, 0, 0, 0)
	};
	let combined_val = ((r1 as u64) << 32) | (r0 as u64);
	Time(combined_val as i64)
}

pub fn request_channel(name: &str) -> Option<Channel> {
	let (retval, handle_id) = unsafe {
		syscall(SysCallMethod::RequestChannel, name.as_ptr() as usize, name.len(), 0)
	};

	if retval != 0 {
		None
	} else {
		Some(Channel { handle: Handle {id: handle_id as u32}})
	}
}

pub fn register_channel_listener(name: &str) -> Option<ChannelListener> {
	let (retval, handle_id) = unsafe {
		syscall(SysCallMethod::RegisterChannelListener, name.as_ptr() as usize, name.len(), 0)
	};

	if retval != 0 {
		None
	} else {
		Some(ChannelListener { handle: Handle {id: handle_id as u32}})
	}
}

pub fn abort(retval: isize) -> ! {
	unsafe {
		syscall(SysCallMethod::Abort, retval as usize, 0, 0);
		unreachable!();
	}
}

#[repr(C)]
enum KernelErrorCodes {
	NoError = 0,
	NotAllowed,
	InvalidHandle,
	Cancelled,
	TimedOut,
	Interrupted,
}

pub enum HandleError {
	InvalidHandle,
	NotAllowed,
}

pub enum WaitError {
	Cancelled,
	TimedOut,
	Interrupted,
	HandleError(HandleError),
	Unknown,
}

trait Waitable {
	fn wait(&self, mask: u32) -> Result<u32, WaitError>;
}

impl Waitable for Channel {

}

impl Waitable for ChannelListener {

}

impl Waitable for PollSet {

}

impl Handle {
	pub fn wait(&self, mask: u32) -> Result<u32, WaitError> {
		let (retval, signals) = unsafe {
			syscall(SysCallMethod::WaitOne, self.id as usize, mask as usize, 0)
		};

		let kec = unsafe {core::mem::transmute::<usize, KernelErrorCodes>(retval)};

		match kec {
			KernelErrorCodes::NoError => Ok(signals as u32),
			KernelErrorCodes::NotAllowed => Err(WaitError::HandleError(HandleError::NotAllowed)),
			KernelErrorCodes::InvalidHandle => Err(WaitError::HandleError(HandleError::InvalidHandle)),
			KernelErrorCodes::Cancelled => Err(WaitError::Cancelled),
			KernelErrorCodes::Interrupted => Err(WaitError::Interrupted),
			_ => Err(WaitError::Unknown)
		}
	}
}

//this depends on mallocing, so it'll have to be moved out of here
impl PollSet {
	pub fn add(&mut self, )
}

impl Drop for Handle {
	fn drop(&mut self) {
		unsafe {
			let (retval, _) = syscall(SysCallMethod::CloseHandle, self.id as usize, 0, 0);
			if retval != 0 {
				panic!("Error closing handle");
			}
		}
	}
}

impl ChannelListener {
	pub fn accept(&self) -> Option<Channel> {

	}
}

#[cfg(test)]
mod tests {
	#[test]
	fn it_works() {
	}
}
