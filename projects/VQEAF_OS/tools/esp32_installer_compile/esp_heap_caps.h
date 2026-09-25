#pragma once
#include <stddef.h>
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 4
inline size_t heap_caps_get_free_size(int){return 128*1024;}
inline size_t heap_caps_get_largest_free_block(int){return 96*1024;}
