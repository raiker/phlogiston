#pragma once

#include "common.h"
#include "pagetable.h"
#include "panic.h"
#include "utility.h"
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
		if (used_pages == max_pages){
			grow_allocation();
		}
		
		uintptr_t page_base = (uintptr_t)data + PAGE_SIZE * used_pages;
	
		if (page_table.allocate(page_base, 1, AllocationGranularity::Page).is_error()){
			panic(PanicCodes::AssertionFailure);
		}
	
		used_pages++;
	
		max_elements = used_pages * PAGE_SIZE / sizeof(T);
	}

	void grow_allocation(){
		//double the size of the allocation, copy old contents across
		uint32_t new_max_pages = max_pages * 2;
	
		auto new_reservation = page_table.reserve(new_max_pages, AllocationGranularity::Page);
		if (new_reservation.is_error()){
			panic(PanicCodes::AssertionFailure);
		}
	
		T* new_data = (T*)new_reservation.get_value();
	
		if (page_table.allocate((uintptr_t)new_data, used_pages, AllocationGranularity::Page).is_error()){
			panic(PanicCodes::AssertionFailure);
		}
	
		memcpy((uintptr_t)new_data, (uintptr_t)data, sizeof(T) * used_elements);
	
		//deallocate and release old storage
		//deallocate pages
		if (page_table.deallocate((uintptr_t)data, used_pages, AllocationGranularity::Page).is_error()){
			panic(PanicCodes::AssertionFailure);
		}
	
		//release pages
		if (page_table.release((uintptr_t)data, max_pages, AllocationGranularity::Page).is_error()){
			panic(PanicCodes::AssertionFailure);
		}
		
		//update state
		max_pages = new_max_pages;
		data = new_data;
	}

	void deallocate_last_page(){
		uintptr_t page_base = (uintptr_t)data + PAGE_SIZE * (used_pages - 1);
	
		if (page_table.deallocate(page_base, 1, AllocationGranularity::Page).is_error()){
			panic(PanicCodes::AssertionFailure);
		}
	
		used_pages--;
	
		max_elements = used_pages * PAGE_SIZE / sizeof(T);
	}
	
public:
	KVector(PageTable &_page_table, uint32_t initial_max_pages = 16) :
		page_table(_page_table),
		max_pages(initial_max_pages)
	{
		//allocate space for the vector
		auto reservation = page_table.reserve(max_pages, AllocationGranularity::Page);
		
		//no convenient way to return failure here...
		if (reservation.is_error()){
			panic(PanicCodes::AssertionFailure);
		}
		
		data = (T*)reservation.get_value();
		used_pages = 0;
		used_elements = 0;
		
		//allocate_another_page();
	}
	
	~KVector() {
		//free array elements
		for (uint32_t i = 0; i < used_elements; i++){
			data[i].~T();
		}
		
		//deallocate pages
		if (page_table.deallocate((uintptr_t)data, used_pages, AllocationGranularity::Page).is_error()){
			panic(PanicCodes::AssertionFailure);
		}
		
		//release pages
		if (page_table.release((uintptr_t)data, max_pages, AllocationGranularity::Page).is_error()){
			panic(PanicCodes::AssertionFailure);
		}
	}
	
	uint32_t size(){
		return used_elements;
	}
	
	template<typename... _Args>
	void emplace_back(_Args&&... __args){
		while (used_elements == max_elements){
			allocate_another_page();
		}
		
		::new((void *)&data[used_elements]) T(std::forward<_Args>(__args)...);
		used_elements++;
	}
	
	template<typename... _Args>
	void emplace(size_t idx, _Args&&... __args){
		if (idx > used_elements){
			panic(PanicCodes::ArrayIndexOutOfBounds);
		}
		
		while (used_elements == max_elements){
			allocate_another_page();
		}
		
		//make space for incoming value
		memcpy((uintptr_t)&data[idx+1], (uintptr_t)&data[idx], (used_elements - idx) * sizeof(T));
		
		::new((void *)&data[idx]) T(std::forward<_Args>(__args)...);
		used_elements++;
	}
	
	T& operator [](size_t idx) {
		if (idx >= used_elements){
			panic(PanicCodes::ArrayIndexOutOfBounds);
		}
		return data[idx];
	}
	
	const T& operator [](size_t idx) const {
		if (idx >= used_elements){
			panic(PanicCodes::ArrayIndexOutOfBounds);
		}
		return data[idx];
	}
	
	void erase(size_t idx) {
		if (idx >= used_elements){
			panic(PanicCodes::ArrayIndexOutOfBounds);
		}
		
		memcpy((uintptr_t)&data[idx], (uintptr_t)&data[idx+1], (used_elements - idx - 1) * sizeof(T));
		used_elements--;
	}
};

