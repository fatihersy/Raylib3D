
#ifndef FMEMORY_H
#define FMEMORY_H

#include "defines.h"

void memory_system_initialize(void);

void* allocate_memory(u64 size, bool will_zero_memory);
void* allocate_memory_linear(u64 size, bool will_zero_memory);

void free_memory(void* block);
void zero_memory(void* block, u64 size);
void* copy_memory(void* dest, const void* source, u64 size);
void* set_memory(void* dest, i32 value, u64 size);

#endif
