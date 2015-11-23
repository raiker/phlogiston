#include "sys_timer.h"
#include "mmio.h"

namespace timer {
	static const uint32_t TIMER_BASE = MMIO_BASE + 0x3000;

	uint64_t get_counter_value() {
		uint32_t upper_i, upper_f, lower;
		
		do {
			upper_i = mmio_read(TIMER_BASE + 0x8);
			lower = mmio_read(TIMER_BASE + 0x4);
			upper_f = mmio_read(TIMER_BASE + 0x8);
		} while (upper_i != upper_f);
		
		return ((uint64_t)upper_f << 32) | lower;
	}
	
	uint32_t get_counter_value_lower(){
		return mmio_read(TIMER_BASE + 0x4);
	}
	
	//interrupts
	static const uint32_t TIMER_INTERVAL = 500000;
	static uint32_t next_interrupt_val;
	
	void init_timer_interrupt(){
		next_interrupt_val = get_counter_value_lower() + TIMER_INTERVAL;
		
		//set comparison value
		mmio_write(TIMER_BASE + 0xc, next_interrupt_val);
		//clear interrupt state
		mmio_write(TIMER_BASE + 0x0, 0x00000001);
		
		//enable the actual interrupt
		//uint32_t IRQ_BASIC_ENABLE = MMIO_BASE + 0xB000 + 0x218;
		//mmio_write(IRQ_BASIC_ENABLE, 0x00000001);
		
		uint32_t IRQ_ENABLE_1 = MMIO_BASE + 0xb000 + 0x210;
		mmio_write(IRQ_ENABLE_1, 0x00000001);
	}
	
	void update_comparison_value(){
		next_interrupt_val += TIMER_INTERVAL;
		
		//set comparison value
		mmio_write(TIMER_BASE + 0xc, next_interrupt_val);
		//clear interrupt state
		mmio_write(TIMER_BASE + 0x0, 0x00000001);
	}
}
