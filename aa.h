#ifndef AA_H
#define AA_H

#include <stddef.h>

// interfaces

typedef void* (*aa_alloc_t)(void* allocator, size_t size);
typedef void (*aa_free_t)(void* allocator, void* ptr);
typedef void (*aa_sweep_t)(void* sweeper);

typedef struct {
  void* allocator;
  aa_alloc_t alloc;
  aa_free_t free;
} aa_allocator_t;

typedef struct {
  void* sweeper;
  aa_alloc_t alloc;
  aa_sweep_t sweep;
} aa_sweeper_t;

void* aa_sweeper_alloc(aa_sweeper_t* sweeper, size_t size);
void aa_sweeper_sweep(aa_sweeper_t* sweeper);

// arena

typedef struct aa_region aa_region_t;

struct aa_region {
  aa_region_t* parent;
  char* ptr;
  char data[];
};

typedef struct {
  int region_size;
  aa_region_t* head;
} aa_arena_t;

aa_arena_t aa_arena_init(size_t region_size);
void aa_arena_deinit(aa_arena_t* arena);
void* aa_arena_alloc(aa_arena_t* arena, size_t size);
void aa_arena_sweep(aa_arena_t* arena);
aa_sweeper_t aa_arena_make_sweeper(aa_arena_t* arena);

#endif // AA_H