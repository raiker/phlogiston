#pragma once
#include "common.h"

namespace timer {
	uint64_t get_counter_value();
	uint32_t get_counter_value_lower();
	
	void init_timer_interrupt();
	void update_comparison_value();
}
