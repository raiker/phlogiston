#include "kmalloc.h"

#include <bitset>
#include "page_alloc.h"
#include "panic.h"

const size_t SMALL_HEAP_MAX_SIZE = 128;
const size_t SMALL_HEAP_GRANULARITY = 4;
const size_t SMALL_HEAP_NUM_SIZES = SMALL_HEAP_MAX_SIZE / SMALL_HEAP_GRANULARITY;
const size_t SMALL_HEAP_MAX_ENTRIES = 1024;

const size_t MEDIUM_HEAP_MAX_SIZE = 2048;
const size_t MEDIUM_HEAP_GRANULARITY = 64;
const size_t MEDIUM_HEAP_SCALE = 32;
const size_t MEDIUM_HEAP_NUM_SIZES = MEDIUM_HEAP_MAX_SIZE / MEDIUM_HEAP_GRANULARITY;
const size_t MEDIUM_HEAP_MAX_ENTRIES = 64;

struct SmallHeapBlockHeader {
	SmallHeapBlockHeader * next;
	uint32_t free_entries;
	std::bitset<SMALL_HEAP_MAX_ENTRIES> utilisation;
	uint32_t entries[];
};

struct MediumHeapBlockHeader {
	MediumHeapBlockHeader * next;
	uint32_t free_entries;
	std::bitset<MEDIUM_HEAP_MAX_ENTRIES> utilisation;
	uint32_t entries[];
};

constexpr uint32_t small_heap_get_num_entries(size_t size){
	return (PAGE_SIZE - sizeof(SmallHeapBlockHeader)) / size;
}

constexpr uint32_t medium_heap_get_num_pages(size_t size){
	return (MEDIUM_HEAP_SCALE * size + PAGE_SIZE - 4) / PAGE_SIZE;
}

constexpr uint32_t medium_heap_get_num_entries(size_t size){
	return (PAGE_SIZE * medium_heap_get_num_pages(size) - sizeof(SmallHeapBlockHeader)) / size;
}

SmallBlockHeader * small_block_heap[SMALL_HEAP_NUM_SIZES] = {0};
MediumBlockHeader * medium_block_heap[MEDIUM_HEAP_NUM_SIZES] = {0};

void kmalloc_init(){
	
}

void * kmalloc(size_t size){
	if (initial_size <= SMALL_HEAP_MAX_SIZE){
		size_t alloc_size = (initial_size + SMALL_HEAP_GRANULARITY - 1) / SMALL_HEAP_GRANULARITY * SMALL_HEAP_GRANULARITY;
		
		SmallHeapBlockHeader ** block_ptr = &small_block_heap[alloc_size / SMALL_HEAP_GRANULARITY - 1];
		
		while (true){
			if (*block_ptr == nullptr){
				//allocate new block
				*block_ptr = page_alloc::alloc();
				(*block_ptr)->next = nullptr;
				(*block_ptr)->free_entries = small_heap_get_num_entries(alloc_size);
			}
			
			if (free_entries > 0){
				for (int i = 0; i < small_heap_get_num_entries(alloc_size); i++){
					if (!(*block_ptr)->utilisation[i]){
						//we've found a free entry
						(*block_ptr)->utilisation[i] = true;
						(*block_ptr)->free_entries--;
						return (void*)&((*block_ptr)->entries[i]);
					}
				}
				panic(PanicCodes::AssertionFailure);
			} else {
				block_ptr = &(*block_ptr)->next;
			}
		}
	} else if (initial_size <= MEDIUM_HEAP_MAX_SIZE){
		size_t alloc_size = (initial_size + MEDIUM_HEAP_GRANULARITY - 1) / MEDIUM_HEAP_GRANULARITY * MEDIUM_HEAP_GRANULARITY;
	} else {
		return initial_size;
	}
}

void kfree(void * memory){
	//free has no observable semantics, so this is a valid implementation
}

/*
#include <iostream>

using namespace std;

int main(){
	for (size_t x = SMALL_HEAP_MAX_SIZE + MEDIUM_HEAP_GRANULARITY; x <= MEDIUM_HEAP_MAX_SIZE; x += MEDIUM_HEAP_GRANULARITY){
		cout << x << " " << medium_heap_get_num_entries(x) << endl;
	}
	return 0;
}
*/

