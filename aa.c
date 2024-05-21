#include "aa.h"
#include <stdio.h>
#include <stdlib.h>

static aa_region_t* init_region(aa_region_t* parent, size_t size);
static void new_region(aa_arena_t* arena);

void* aa_sweeper_alloc(aa_sweeper_t* sweeper, size_t size) {
  return sweeper->alloc(sweeper->sweeper, size);
}

void aa_sweeper_sweep(aa_sweeper_t* sweeper) {
  sweeper->sweep(sweeper->sweeper);
}

aa_arena_t aa_arena_init(size_t region_size) {
  aa_arena_t arena = (aa_arena_t){.head = NULL, .region_size = region_size};
  new_region(&arena);
  return arena;
}

void aa_arena_deinit(aa_arena_t* arena) {
  while (arena->head != NULL) {
    aa_region_t* head = arena->head;
    arena->head = head->parent;
    free(head);
  }
}

void* aa_arena_alloc(aa_arena_t* arena, size_t size) {
  if (arena->head->ptr + size > arena->head->data + arena->region_size) {
    new_region(arena);
    if (arena->head->ptr + size > arena->head->data + arena->region_size) {
      return NULL;
    }
  }
  void* ptr = arena->head->ptr;
  arena->head->ptr += size;
  return ptr;
}

void aa_arena_sweep(aa_arena_t* arena) {
  aa_arena_deinit(arena);
  new_region(arena);
}

aa_sweeper_t aa_arena_make_sweeper(aa_arena_t* arena) {
  return (aa_sweeper_t){
    .sweeper = arena,
    .alloc = (aa_alloc_t)aa_arena_alloc,
    .sweep = (aa_sweep_t)aa_arena_sweep,
  };
}

static aa_region_t* init_region(aa_region_t* parent, size_t size) {
  aa_region_t* region = malloc(size + 8);
  region->parent = parent;
  region->ptr = region->data;
  return region;
}

static void new_region(aa_arena_t* arena) {
  arena->head = init_region(arena->head, arena->region_size);
}