#pragma once

#include "common.h"

namespace atags {
	void debug_atags(void * base_ptr);
	MemRange get_mem_range(void * base_ptr);
}

