#include "atags.h"

#include "uart.h"

const uint32_t ATAG_NONE = 0x00000000;
const uint32_t ATAG_CORE = 0x54410001;
const uint32_t ATAG_MEM = 0x54410002;
const uint32_t ATAG_VIDEOTEXT = 0x54410003;
const uint32_t ATAG_RAMDISK = 0x54410004;
const uint32_t ATAG_INITRD2 = 0x54410005;
const uint32_t ATAG_SERIAL = 0x54410006;
const uint32_t ATAG_REVISION = 0x54410007;
const uint32_t ATAG_VIDEOLFB = 0x54410008;
const uint32_t ATAG_CMDLINE = 0x54410009;

struct atag_header {
	uint32_t size; /* legth of tag in words including this header */
	uint32_t tag;  /* tag value */
};

struct atag_core {
	uint32_t flags;              /* bit 0 = read-only */
	uint32_t pagesize;           /* systems page size (usually 4k) */
	uint32_t rootdev;            /* root device number */
};

struct atag_mem {
	uint32_t     size;   /* size of the area */
	uint32_t     start;  /* physical start address */
};

struct atag {
	struct atag_header hdr;
	union {
		struct atag_core         core;
		struct atag_mem          mem;
		/**struct atag_videotext    videotext;
		struct atag_ramdisk      ramdisk;
		struct atag_initrd2      initrd2;
		struct atag_serialnr     serialnr;
		struct atag_revision     revision;
		struct atag_videolfb     videolfb;
		struct atag_cmdline      cmdline;*/
	};
};

void debug_atags(void * base_ptr){
	uint32_t *tag_base = (uint32_t *)base_ptr;
	
	while (true){
		atag * ctag = (atag*) tag_base;
		
		switch (ctag->hdr.tag){
			case ATAG_NONE:
				return;
			case ATAG_CORE:
				uart_puts("Core tag\r\n");
				uart_puts("Flags: ");
				uart_puthex(ctag->core.flags);
				uart_puts("\r\n");
				uart_puts("Pagesize: ");
				uart_puthex(ctag->core.pagesize);
				uart_puts("\r\n");
				uart_puts("Rootdev: ");
				uart_puthex(ctag->core.rootdev);
				uart_puts("\r\n");
				break;
			case ATAG_MEM:
				uart_puts("Memory tag\r\n");
				uart_puts("Size: ");
				uart_puthex(ctag->mem.size);
				uart_puts("\r\n");
				uart_puts("Start: ");
				uart_puthex(ctag->mem.start);
				uart_puts("\r\n");
				break;
			default:
				uart_puts("Other tag\r\n");
				break;
		}
		
		tag_base += ctag->hdr.size;
	}
}
