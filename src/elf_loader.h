#pragma once

#include "pagetable.h"
#include "common.h"

bool load_elf(void * elf_header, PageTable & pagetable, void ** entry_address);
void elf_parse_header(void * elf_header);

