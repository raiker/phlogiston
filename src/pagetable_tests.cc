#include "runtime_tests.h"
#include "pagetable.h"

bool test_reservations() {
	bool all_passed = true;

	PrePagingPageTable table(true);
	
	//trivial reservations
	all_passed &= table.reserve(0x10000000, 1, AllocationGranularity::Page).is_success;
	all_passed &= table.reserve(0x20000000, 1, AllocationGranularity::Section).is_success;
	all_passed &= table.reserve(0x30000000, 1, AllocationGranularity::Supersection).is_success;
	
	//reservation of page in partially-reserved second-level table
	all_passed &= table.reserve(0x10001000, 1, AllocationGranularity::Page).is_success;
	
	//multi-page reservation
	all_passed &= table.reserve(0x11000000, 5, AllocationGranularity::Page).is_success;
	
	//clashing reservation
	all_passed &= not table.reserve(0x10000000, 1, AllocationGranularity::Section).is_success;
	
	//multi-page reservation in partially-reserved second-level table
	all_passed &= table.reserve(0x10007000, 5, AllocationGranularity::Page).is_success;
	
	//overflowing reservation
	all_passed &= table.reserve(0x12000000, 300, AllocationGranularity::Page).is_success;
	
	//overflowing reservation in partially-reserved second-level table
	all_passed &= table.reserve(0x10010000, 300, AllocationGranularity::Page).is_success;
	
	//partially-overlapping reservation
	all_passed &= not table.reserve(0x11fff000, 4, AllocationGranularity::Page).is_success;
	
	table.print_table_info();
	
	return all_passed;
}

bool test_pagetables() {
	bool all_passed = true;
	
	//non-short circuit
	all_passed &= test_reservations();
	
	return all_passed;
}
