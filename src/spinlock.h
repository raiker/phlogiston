#pragma once

#include "common.h"

#include <atomic>

class Spinlock {
	std::atomic_flag flag = ATOMIC_FLAG_INIT;
public:
	
	class HeldLockDummy {
		friend class Spinlock;
		Spinlock &parent;
		
		HeldLockDummy(Spinlock &_parent);
		
	public:
		~HeldLockDummy();
	};
	
	HeldLockDummy acquire();
};

