#include "elf_loader.h"
#include "uart.h"

#define EI_NIDENT 16

typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef int32_t Elf32_Sword;
typedef uint16_t Elf32_Half;

struct Elf32_Ehdr {
	unsigned char   e_ident[EI_NIDENT];
	Elf32_Half      e_type;
	Elf32_Half      e_machine;
	Elf32_Word      e_version;
	Elf32_Addr      e_entry;
	Elf32_Off       e_phoff;
	Elf32_Off       e_shoff;
	Elf32_Word      e_flags;
	Elf32_Half      e_ehsize;
	Elf32_Half      e_phentsize;
	Elf32_Half      e_phnum;
	Elf32_Half      e_shentsize;
	Elf32_Half      e_shnum;
	Elf32_Half      e_shstrndx;
};

enum Elf32_SectionType : Elf32_Word {
	SHT_NULL = 0,
	SHT_PROGBITS = 1,
	SHT_SYMTAB = 2,
	SHT_STRTAB = 3,
	SHT_RELA = 4,
	SHT_HASH = 5,
	SHT_DYNAMIC = 6,
	SHT_NOTE = 7,
	SHT_NOBITS = 8,
	SHT_REL = 9,
	SHT_SHLIB = 10,
	SHT_DYNSYM = 11,
	SHT_INIT_ARRAY = 14,
	SHT_FINI_ARRAY = 15,
	SHT_PREINIT_ARRAY = 16,
	SHT_GROUP = 17,
	SHT_SYMTAB_SHNDX = 18,
	SHT_LOOS = 0x60000000,
	SHT_HIOS = 0x6fffffff,
	SHT_LOPROC = 0x70000000,
	SHT_HIPROC = 0x7fffffff,
	SHT_LOUSER = 0x80000000,
	SHT_HIUSER = 0xffffffff,
};

enum Elf32_SectionFlags : Elf32_Word {
	SHF_WRITE = 0x1,
	SHF_ALLOC = 0x2,
	SHF_EXECINSTR = 0x4,
	SHF_MERGE = 0x10,
	SHF_STRINGS = 0x20,
	SHF_INFO_LINK = 0x40,
	SHF_LINK_ORDER = 0x80,
	SHF_OS_NONCONFORMING = 0x100,
	SHF_GROUP = 0x200,
	SHF_TLS = 0x400,
	SHF_COMPRESSED = 0x800,
	SHF_MASKOS = 0x0ff00000,
	SHF_MASKPROC = 0xf0000000,
};

struct Elf32_Shdr {
	Elf32_Word	sh_name;
	Elf32_SectionType	sh_type;
	Elf32_SectionFlags	sh_flags;
	Elf32_Addr	sh_addr;
	Elf32_Off	sh_offset;
	Elf32_Word	sh_size;
	Elf32_Word	sh_link;
	Elf32_Word	sh_info;
	Elf32_Word	sh_addralign;
	Elf32_Word	sh_entsize;
};

void elf_parse_header(void * elf_header){
	Elf32_Ehdr *header = (Elf32_Ehdr*)elf_header;
	
	uart_puts("Entry point: "); uart_puthex((uint32_t)header->e_entry); uart_puts("\r\n");
	uart_puts("Program Header Offset: "); uart_puthex((uint32_t)header->e_phoff); uart_puts("\r\n");
	uart_puts("Section Header Offset: "); uart_puthex((uint32_t)header->e_shoff); uart_puts("\r\n");
	
	if (sizeof(Elf32_Shdr) != header->e_shentsize){
		uart_puts("Size mismatch\r\n");
		return;
	}
	
	Elf32_Shdr * section_header_array = (Elf32_Shdr*)((uintptr_t)elf_header + header->e_shoff);
	
	for (uint32_t i = 0; i < header->e_shnum; i++){
		const Elf32_Shdr & sec_header = section_header_array[i];
		
		if (sec_header.sh_type == Elf32_SectionType::SHT_PROGBITS){
			if (sec_header.sh_flags & Elf32_SectionFlags::SHF_ALLOC){
				uart_puts("Section\r\nAddr: ");
				uart_puthex(sec_header.sh_addr);
				uart_puts("\r\nOffset: ");
				uart_puthex(sec_header.sh_offset);
				uart_puts("\r\nSize: ");
				uart_puthex(sec_header.sh_size);
				uart_puts("\r\n\r\n");
			}
		}
	}
}

bool load_elf(void * elf_header, PageTable & pagetable) {
	Elf32_Ehdr *header = (Elf32_Ehdr*)elf_header;
	
	if (sizeof(Elf32_Shdr) != header->e_shentsize){
		uart_puts("Size mismatch\r\n");
		return false;
	}
	
	Elf32_Shdr * section_header_array = (Elf32_Shdr*)((uintptr_t)elf_header + header->e_shoff);
	
	for (uint32_t i = 0; i < header->e_shnum; i++){
		const Elf32_Shdr & sec_header = section_header_array[i];
		
		if (sec_header.sh_type == Elf32_SectionType::SHT_PROGBITS){
			if (sec_header.sh_flags & Elf32_SectionFlags::SHF_ALLOC){
				//check section alignment
				if ((sec_header.sh_offset & (PAGE_SIZE - 1)) || (sec_header.sh_addr & (PAGE_SIZE - 1))){
					uart_puts("Unaligned section\r\n");
					return false;
				}
				
				uint32_t npages = get_num_allocation_units(sec_header.sh_size, AllocationGranularity::Page);
				auto reservation = pagetable.reserve(sec_header.sh_addr, npages, AllocationGranularity::Page);
				
				if (!reservation.is_success){
					uart_puts("Failed to reserve memory\r\n");
					return false;
				}
				
				//TODO: make this able to cope with the header being paged
				uintptr_t physical_addr = (uintptr_t)header + sec_header.sh_offset;
				
				if (!pagetable.map(reservation.value, physical_addr, npages, AllocationGranularity::Page)){
					uart_puts("Failed to map memory\r\n");
					return false;
				}
			}
		}
	}
	
	return true;
}

