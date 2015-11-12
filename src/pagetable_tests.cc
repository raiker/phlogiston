#include "runtime_tests.h"
#include "pagetable.h"
#include "uart.h"

bool test_reservations() {
	bool all_passed = true;

	PrePagingPageTable table(true);
	
	uart_puts("cats!\r\n");
	
	//reserve single page
	uart_puts("Single page reservation: ");
	{
		auto check = table.get_unit_state(0x10000000, AllocationGranularity::Page);
		all_passed &= check.is_success && check.value == UnitState::Free;
	}
	all_passed &= table.reserve(0x10000000, 1, AllocationGranularity::Page).is_success;
	{
		auto check = table.get_unit_state(0x10000000, AllocationGranularity::Page);
		all_passed &= check.is_success && check.value == UnitState::Reserved;
	}
	if (all_passed) {
		uart_puts("passed\r\n");
	} else {
		uart_puts("failed\r\n");
	}
	
	
	//reserve single section
	uart_puts("Single section reservation: ");
	{
		auto check = table.get_unit_state(0x20000000, AllocationGranularity::Section);
		all_passed &= check.is_success && check.value == UnitState::Free;
	}
	all_passed &= table.reserve(0x20000000, 1, AllocationGranularity::Section).is_success;
	{
		auto check = table.get_unit_state(0x20000000, AllocationGranularity::Section);
		all_passed &= check.is_success && check.value == UnitState::Reserved;
	}
	if (all_passed) {
		uart_puts("passed\r\n");
	} else {
		uart_puts("failed\r\n");
	}
	
	
	//reserve single supersection
	uart_puts("Single supersection reservation: ");
	{
		auto check = table.get_unit_state(0x30000000, AllocationGranularity::Supersection);
		all_passed &= check.is_success && check.value == UnitState::Free;
	}
	all_passed &= table.reserve(0x30000000, 1, AllocationGranularity::Supersection).is_success;
	{
		auto check = table.get_unit_state(0x30000000, AllocationGranularity::Supersection);
		all_passed &= check.is_success && check.value == UnitState::Reserved;
	}
	if (all_passed) {
		uart_puts("passed\r\n");
	} else {
		uart_puts("failed\r\n");
	}
	
	
	//reservation of page in partially-reserved second-level table
	uart_puts("Reservation of page in partially-reserved second-level table: ");
	{
		auto check = table.get_unit_state(0x10001000, AllocationGranularity::Page);
		all_passed &= check.is_success && check.value == UnitState::Free;
	}
	all_passed &= table.reserve(0x10001000, 1, AllocationGranularity::Page).is_success;
	{
		auto check = table.get_unit_state(0x10001000, AllocationGranularity::Page);
		all_passed &= check.is_success && check.value == UnitState::Reserved;
	}
	if (all_passed) {
		uart_puts("passed\r\n");
	} else {
		uart_puts("failed\r\n");
	}
	
	
	//multi-page reservation
	uart_puts("Multi-page reservation: ");
	{
		for (uint32_t i = 0, addr = 0x11000000; i < 5; i++, addr += 0x1000){
			auto check = table.get_unit_state(addr, AllocationGranularity::Page);
			all_passed &= check.is_success && check.value == UnitState::Free;
		}
	}
	all_passed &= table.reserve(0x11000000, 5, AllocationGranularity::Page).is_success;
	{
		for (uint32_t i = 0, addr = 0x11000000; i < 5; i++, addr += 0x1000){
			auto check = table.get_unit_state(addr, AllocationGranularity::Page);
			all_passed &= check.is_success && check.value == UnitState::Reserved;
		}
	}
	if (all_passed) {
		uart_puts("passed\r\n");
	} else {
		uart_puts("failed\r\n");
	}
	
	
	//clashing reservation
	uart_puts("Clashing reservation: ");
	all_passed &= not table.reserve(0x10000000, 1, AllocationGranularity::Section).is_success;
	if (all_passed) {
		uart_puts("passed\r\n");
	} else {
		uart_puts("failed\r\n");
	}
	
	//multi-page reservation in partially-reserved second-level table
	uart_puts("Multi-page reservation in partially-reserved second-level table: ");
	{
		for (uint32_t i = 0, addr = 0x10007000; i < 5; i++, addr += 0x1000){
			auto check = table.get_unit_state(addr, AllocationGranularity::Page);
			all_passed &= check.is_success && check.value == UnitState::Free;
		}
	}
	all_passed &= table.reserve(0x10007000, 5, AllocationGranularity::Page).is_success;
	{
		for (uint32_t i = 0, addr = 0x10007000; i < 5; i++, addr += 0x1000){
			auto check = table.get_unit_state(addr, AllocationGranularity::Page);
			all_passed &= check.is_success && check.value == UnitState::Reserved;
		}
	}
	if (all_passed) {
		uart_puts("passed\r\n");
	} else {
		uart_puts("failed\r\n");
	}
	
	//overflowing reservation
	uart_puts("Overflowing reservation: ");
	{
		for (uint32_t i = 0, addr = 0x12000000; i < 300; i++, addr += 0x1000){
			auto check = table.get_unit_state(addr, AllocationGranularity::Page);
			all_passed &= check.is_success && check.value == UnitState::Free;
		}
	}
	all_passed &= table.reserve(0x12000000, 300, AllocationGranularity::Page).is_success;
	{
		for (uint32_t i = 0, addr = 0x12000000; i < 300; i++, addr += 0x1000){
			auto check = table.get_unit_state(addr, AllocationGranularity::Page);
			all_passed &= check.is_success && check.value == UnitState::Reserved;
		}
	}
	if (all_passed) {
		uart_puts("passed\r\n");
	} else {
		uart_puts("failed\r\n");
	}
	
	//overflowing reservation in partially-reserved second-level table
	uart_puts("Overflowing reservation in partially-reserved second-level table: ");
	{
		for (uint32_t i = 0, addr = 0x10010000; i < 300; i++, addr += 0x1000){
			auto check = table.get_unit_state(addr, AllocationGranularity::Page);
			all_passed &= check.is_success && check.value == UnitState::Free;
		}
	}
	all_passed &= table.reserve(0x10010000, 300, AllocationGranularity::Page).is_success;
	{
		for (uint32_t i = 0, addr = 0x10010000; i < 300; i++, addr += 0x1000){
			auto check = table.get_unit_state(addr, AllocationGranularity::Page);
			all_passed &= check.is_success && check.value == UnitState::Reserved;
		}
	}
	if (all_passed) {
		uart_puts("passed\r\n");
	} else {
		uart_puts("failed\r\n");
	}
	
	//partially-overlapping reservation
	uart_puts("Partially-overlapping reservation: ");
	all_passed &= not table.reserve(0x11fff000, 4, AllocationGranularity::Page).is_success;
	if (all_passed) {
		uart_puts("passed\r\n");
	} else {
		uart_puts("failed\r\n");
	}
	
	uart_putline();
	
	table.print_table_info();
	
	return all_passed;
}

bool test_pagetables() {
	bool all_passed = true;
	
	//non-short circuit
	all_passed &= test_reservations();
	
	return all_passed;
}
