#pragma once

#include "kvector.h"
#include "panic.h"

template<typename T>
class KRollingQueue {
	KVector<T> store;
	size_t read_head;
	
public:
	KRollingQueue(PageTable &_page_table, uint32_t initial_max_pages = 16) :
		store(_page_table, initial_max_pages),
		read_head(0)
	{ }
	
	const T & next() {
		if (store.size() == 0){
			panic(PanicCodes::ArrayIndexOutOfBounds);
		}
		
		T &x = store[read_head];
		read_head = (read_head + 1) % store.size();
		return x;
	}
	
	//inserts element directly in front of read head, so it will be the next to be returned
	template<typename... _Args>
	void add(_Args&&... __args){
		store.emplace(read_head, std::forward<_Args>(__args)...);
		used_elements++;
	}
};
