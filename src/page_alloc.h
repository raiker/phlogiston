#pragma once

#include "common.h"

namespace page_alloc {
	struct MemStats {
		uint32_t totalmem;
		uint32_t freemem;
		uint32_t usedmem;
	};
	
	void init(uint32_t total_memory);
	uintptr_t alloc(uint32_t size);
	uint32_t ref_acquire(uintptr_t page);
	uint32_t ref_release(uintptr_t page);
	void ref_release(uintptr_t page, uint32_t size);
	MemStats get_mem_stats();
}
