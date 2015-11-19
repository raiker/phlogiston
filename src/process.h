#pragma once

#include "common.h"
#include "pagetable.h"
#include "kvector.h"

const uint32_t STACK_NUM_PAGES = 16; //64KiB

class Process;

struct Stack {
	uintptr_t base;
	uint32_t num_pages;
};

class Thread {
	Stack stack;
	Process &process;
	
	uint32_t registers[16]; //registers[15] is used to hold the CPSR; PC is stored on the stack
public:
	Thread(Process &_process, Stack _stack, uintptr_t entry_point);
};

class Process {
	PageTable page_table;
	KVector<Thread> threads;
	
public:
	Process(PageAlloc &page_alloc) :
		page_table(page_alloc, false),
		threads(page_table)
	{}
	
	bool create_thread(uintptr_t entry_point);
};

