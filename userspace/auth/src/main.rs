#![no_std]

extern crate sysiface;

use sysiface::prelude::*;

extern fn _start() -> ! {
	main();
	sysiface::abort(0);
}

fn main() {
	//open auth channel
	let auth_channel = sysiface::request_channel("phlogiston::auth").expect("Failed to open authentication channel");

	loop {
		auth_channel.wait()
	}
}
