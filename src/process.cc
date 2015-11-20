#include "process.h"

Thread::Thread(Process &_process, Stack _stack, uintptr_t entry_point) :
	stack(_stack),
	process(_process)
{
	for (uint32_t i = 0; i < 16; i++){
		registers[i] = 0x00000000;
	}
	
	registers[14] = entry_point; //write to LR (might need to be changed)
}

bool Process::create_thread(uintptr_t entry_point){
	auto stack_allocation = page_table.reserve_allocate(STACK_NUM_PAGES, AllocationGranularity::Page);
	if (stack_allocation.is_error()){
		return false;
	}
	Stack new_stack = {stack_allocation.get_value(), STACK_NUM_PAGES};
	
	threads.emplace_back(*this, new_stack, entry_point);
	
	return true;
}
