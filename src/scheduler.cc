#include "scheduler.h"

void Scheduler::next_thread() {
	size_t num_threads = threads.size();
	
	for (size_t = 0; i < num_threads; i++){
		ThreadState &ts = threads.next();
		
		if (!ts.is_blocked){
			//this is our new thread to run
			
		}
	}
}
