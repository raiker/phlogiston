#include "elf_loader.h"
#include "uart.h"
#include "elf.h"
#include "panic.h"
#include "utility.h"

void elf_parse_header(void * elf_header){
	Elf32_Ehdr *header = (Elf32_Ehdr*)elf_header;
	
	uart_puts("Entry point: "); uart_puthex((uint32_t)header->e_entry); uart_puts("\r\n");
	uart_puts("Program Header Offset: "); uart_puthex((uint32_t)header->e_phoff); uart_puts("\r\n");
	uart_puts("Section Header Offset: "); uart_puthex((uint32_t)header->e_shoff); uart_puts("\r\n");
	
	if (sizeof(Elf32_Shdr) != header->e_shentsize){
		uart_puts("Size mismatch\r\n");
		return;
	}
	
	Elf32_Phdr * program_header_array = (Elf32_Phdr*)((uintptr_t)elf_header + header->e_phoff);
	
	for (uint32_t i = 0; i < header->e_phnum; i++){
		const Elf32_Phdr & seg_header = program_header_array[i];
		
		if (seg_header.p_type == PT_LOAD){
			uart_puts("Program Segment\r\nAddr: ");
			uart_puthex(seg_header.p_vaddr);
			uart_puts("\r\nOffset: ");
			uart_puthex(seg_header.p_offset);
			uart_puts("\r\nSize: ");
			uart_puthex(seg_header.p_memsz);
			uart_puts("\r\nSize to copy: ");
			uart_puthex(seg_header.p_filesz);
			uart_puts("\r\nAlign: ");
			uart_puthex(seg_header.p_align);
			uart_puts("\r\n\r\n");
		}
	}
	
	/*Elf32_Shdr * section_header_array = (Elf32_Shdr*)((uintptr_t)elf_header + header->e_shoff);
	
	for (uint32_t i = 0; i < header->e_shnum; i++){
		const Elf32_Shdr & sec_header = section_header_array[i];
		
		if (sec_header.sh_type == SHT_PROGBITS){
			if (sec_header.sh_flags & SHF_ALLOC){
				uart_puts("Section\r\nAddr: ");
				uart_puthex(sec_header.sh_addr);
				uart_puts("\r\nOffset: ");
				uart_puthex(sec_header.sh_offset);
				uart_puts("\r\nSize: ");
				uart_puthex(sec_header.sh_size);
				uart_puts("\r\n\r\n");
			}
		}
	}*/
}

//TODO: make this able to handle arbitrary numbers of sections
bool load_elf(void * elf_header, PageTable & pagetable, void ** entry_address) {
	Elf32_Ehdr *header = (Elf32_Ehdr*)elf_header;
	
	if (sizeof(Elf32_Phdr) != header->e_phentsize){
		uart_puts("Size mismatch\r\n");
		return false;
	}
	if (sizeof(Elf32_Shdr) != header->e_shentsize){
		uart_puts("Size mismatch\r\n");
		return false;
	}

	if (header->e_shnum == 0xffff){
		uart_puts("Too many sections");
		return false;
	}
		
	Elf32_Phdr * program_header_array = (Elf32_Phdr*)((uintptr_t)elf_header + header->e_phoff);
	
	for (uint32_t i = 0; i < header->e_phnum; i++){
		const Elf32_Phdr & seg_header = program_header_array[i];
		
		if (seg_header.p_type == PT_LOAD){
			if (seg_header.p_vaddr & (PAGE_SIZE - 1)){
				uart_puts("Unaligned segment\r\n");
				return false;
			}
			
			uint32_t npages = get_num_allocation_units(seg_header.p_memsz, AllocationGranularity::Page);
			
			auto allocation = pagetable.reserve_allocate(seg_header.p_vaddr, npages, AllocationGranularity::Page);
			
			if (!allocation.is_success){
				return false;
			}
		}
	}
	
	Elf32_Shdr * section_header_array = (Elf32_Shdr*)((uintptr_t)elf_header + header->e_shoff);
	
	for (uint32_t i = 0; i < header->e_shnum; i++){
		const Elf32_Shdr & sec_header = section_header_array[i];
		
		if (sec_header.sh_type == SHT_PROGBITS){
			if (sec_header.sh_flags & SHF_ALLOC){
				//TODO: fix gaping DoS attack surface
				uintptr_t source_addr = (uintptr_t)elf_header + sec_header.sh_offset;
				//Result<uintptr_t> dest_addr = pagetable.virtual_to_physical(sec_header.sh_addr);
				uintptr_t dest_addr = sec_header.sh_addr;
				size_t size = sec_header.sh_size;
				
				/*uart_puts("Copy ");
				uart_puthex(size);
				uart_puts(" bytes from ");
				uart_puthex(source_addr);
				uart_puts(" to ");
				uart_puthex(dest_addr);
				uart_putline();*/
				memcpy(dest_addr, source_addr, size);
			}
		}
	}
	
	*entry_address = (void*)header->e_entry;
	
	return true;
}

