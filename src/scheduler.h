#pragma once

#include "common.h"
#include "process.h"
#include "krolling_queue.h"
#include <atomic>

class Scheduler {
private:
	struct ThreadState {
		Thread *thread;
		bool is_blocked;
	};
	
	KRollingQueue<*Thread> threads;
public:
	void next_thread();
};
