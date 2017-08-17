#![no_std]

extern crate sysiface;

use sysiface::prelude::*;

extern fn _start() -> ! {
	main();
	sysiface::abort(0);
}

fn main() {
	//register channel listener
	let auth_channel_listener = sysiface::register_channel_listener("phlogiston.auth").expect("Failed to register authentication channel listener");
	
	//open sysevent channel
	let sysevent_channel = sysiface::request_channel("phlogiston.sysevent").expect("Failed to open system event channel");

	let mut pollset = sysiface::poll_set::new();
	pollset.add(auth_channel_listener);
	pollset.add(sysevent_channel);

	loop {
		let signalling_handles = pollset.wait(sysiface::READABLE | sysiface::CLOSED);

		for h in signalling_handles {
			h.
		}
	}
}
