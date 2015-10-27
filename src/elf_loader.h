#pragma once

#include "pagetable.h"
#include "common.h"

//void load_elf(void * header, PageTable & pagetable);
void elf_parse_header(void * elf_header);

