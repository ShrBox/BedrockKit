#pragma once
#include<cstdint>
#include<cstring>
#include<cstdio>
#include<cmath>
#include<typeinfo>
template<typename pattern_type,unsigned int pattern_sz,unsigned int hint,unsigned int MAXSZ>
struct MSearcher {
	unsigned int _Off;
	template<typename T>
	void Init(T* mem, void* payload) {
		if (memcmp(((char*)mem) + hint, payload, pattern_sz) == 0) {
			_Off = hint;
			return;
		}
		printf("[WARN] MSearch Hint error for %s:%s\n", typeid(T*).name(),typeid(pattern_type*).name());
		unsigned int Hit = 0,LBound=0,UBound=0;
		if (hint >= 128)
			LBound = hint - 128;
		UBound = hint + 128;
		if (UBound > MAXSZ - pattern_sz + 1)
			UBound = MAXSZ - pattern_sz + 1;
		for (; LBound < UBound; ++LBound) {
			if (memcmp(((char*)mem) + hint, payload, pattern_sz) == 0) {
				if (Hit) {
					printf("[ERROR] multiple MSearch HIT for %s:%s %u vs %u\n", typeid(T*).name(), typeid(pattern_type*).name(), Hit, LBound);
					if (abs((long)LBound - (long)hint) <= abs((long)Hit - (long)hint)) {
						Hit = LBound;
					}
					//return;
				}
				else {
					Hit = LBound;
				}
			}
		}
		if (Hit == 0) {
			printf("[ERROR] multiple MSearch Failed for %s:%s\n", typeid(T*).name(), typeid(pattern_type*).name());
			exit(1);
		}
		_Off = Hit;
	}
	pattern_type& get(void* mem) {
		return *(pattern_type*)(((char*)mem) + _Off);
	}
};