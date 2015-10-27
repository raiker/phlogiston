#include "spinlock.h"

Spinlock::HeldLockDummy::HeldLockDummy(Spinlock &_parent) :
	parent(_parent) {}

Spinlock::HeldLockDummy::~HeldLockDummy() {
	parent.flag.clear(std::memory_order_release);
}

Spinlock::HeldLockDummy Spinlock::acquire() {
	while (flag.test_and_set(std::memory_order_acquire)) {
	}
	
	return Spinlock::HeldLockDummy(*this);
}
