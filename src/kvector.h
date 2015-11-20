#pragma once

#include "common.h"
#include "pagetable.h"
#include "panic.h"
#include <utility>

inline void* operator new(size_t sz, void* here){
	(void)sz;
	return here;
}

template<typename T>
class KVector {
	PageTable &page_table;
	uint32_t used_elements;
	uint32_t max_elements;
	uint32_t used_pages;
	uint32_t max_pages;
	T * data;
	
void allocate_another_page(){
	uintptr_t page_base = (uintptr_t)data + PAGE_SIZE * used_pages;
	
	if (page_table.allocate(page_base, 1, AllocationGranularity::Page).is_error()){
		panic(PanicCodes::AssertionFailure);
	}
	
	used_pages++;
	
	max_elements = used_pages * PAGE_SIZE / sizeof(T);
}
	
public:
	KVector(PageTable &_page_table) :
		page_table(_page_table)
	{
		//allocate space for the vector
		auto reservation = page_table.reserve(1, AllocationGranularity::Section);
		
		//no convenient way to return failure here...
		if (reservation.is_error()){
			panic(PanicCodes::AssertionFailure);
		}
		
		max_pages = SECTION_SIZE / PAGE_SIZE;
		used_pages = 0;
		
		allocate_another_page();
		
		used_elements = 0;
		
		data = (T*)reservation.get_value();
	}
	
	~KVector() {
		for (uint32_t i = 0; i < used_elements; i++){
			data[i].~T();
		}
		//TODO: free pages
	}
	
	uint32_t size(){
		return used_elements;
	}
	
	template<typename... _Args>
	void emplace_back(_Args&&... __args){
		if (used_elements == max_elements){
			allocate_another_page();
		}
		
		::new((void *)&data[used_elements]) T(std::forward<_Args>(__args)...);
		used_elements++;
	}
};

