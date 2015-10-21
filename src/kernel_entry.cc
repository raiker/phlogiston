#include "pagetable.h"
#include "panic.h"

extern "C"
void kernel_entry(PageTable * supervisor_pagetable, void * physical_memory_alloc /*fixme*/) {
	panic(PanicCodes::AssertionFailure);
}
