#pragma once

#include "common.h"

namespace page_alloc {
	void init(uint32_t total_memory);
	uintptr_t alloc();
	uint32_t ref_acquire(uintptr_t page);
	uint32_t ref_release(uintptr_t page);
}
