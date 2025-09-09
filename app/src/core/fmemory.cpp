#include "fmemory.h"

#include <stdlib.h> // Required for: malloc(), free()
#include <string.h> // Required for: memset(), memcpy()

typedef struct memory_system_state {
    u64 linear_memory_total_size;
    u64 linear_memory_allocated;
    void *linear_memory;
  } memory_system_state;

static memory_system_state* memory_system;

void memory_system_initialize(void) {
    memory_system = (memory_system_state*)malloc(sizeof(memory_system_state));
    memory_system->linear_memory_total_size = TOTAL_ALLOCATED_MEMORY;
    memory_system->linear_memory_allocated = 0;
    memory_system->linear_memory = 0;

    memory_system->linear_memory = malloc(memory_system->linear_memory_total_size);
}

void* allocate_memory_linear(u64 size, bool will_zero_memory) {
    #ifdef _DEBUG
      if (memory_system->linear_memory_allocated + size > memory_system->linear_memory_total_size) {
        exit(EXIT_FAILURE);
      }
      if (size % sizeof(size_t) != 0) {
        exit(EXIT_FAILURE);
      }
    #endif

    void* block = ((u8*)memory_system->linear_memory) + memory_system->linear_memory_allocated;
    memory_system->linear_memory_allocated += size;

    if (will_zero_memory) memset(block, 0, size);

    return block;
}

void* allocate_memory(u64 size, bool will_zero_memory) {
    void* block = malloc(size);

    if (block == NULL) {
        // TODO: 
        exit(EXIT_FAILURE);
    }

    if (will_zero_memory) memset(block, 0, size);

    return block;
}

void free_memory(void* block) {
    free(block);
}

void zero_memory(void* block, u64 size) {
    memset(block, 0, size);
}

void* copy_memory(void* dest, const void* source, u64 size) {
    return memcpy(dest, source, size);
}

void* set_memory(void* dest, i32 value, u64 size) {
    return memset(dest, value, size);
}
