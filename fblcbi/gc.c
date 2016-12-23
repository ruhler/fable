// gc.c --
//
//   This file implements routines for setting up and tearing down the GC
//   infrastructure.

#define GC_DEBUG
#include <gc/gc.h>

#include "gc.h"

// AllocList --
//   A list of allocations, used for keeping track of allocations made by a
//   BulkFreeArena so they can be explicitly freed when no longer needed. The
//   list nodes are themselves the allocations they are keeping track of, with
//   a header item pointing to the next allocation.
typedef struct BulkFreeArenaAllocList {
  struct BulkFreeArenaAllocList* next;
  uint8_t data[];
} BulkFreeArenaAllocList;

// BulkFreeArena --
//   Implements the bulk-free arena.
typedef struct {
  void* (*alloc)(struct FblcArena* this, size_t size);
  void (*free)(struct FblcArena* this, void* ptr);
  FblcArena* arena;
  BulkFreeArenaAllocList* allocs;
} BulkFreeArena;

static void* GcArenaAlloc(FblcArena* this, size_t size);
static void GcArenaFree(FblcArena* this, void* ptr);
static void* BulkFreeArenaAlloc(FblcArena* this, size_t size);
static void BulkFreeArenaFree(FblcArena* this, void* ptr);

// GcInit - see documentation in gc.h
void GcInit()
{
  GC_find_leak = 1;
  GC_INIT();
}

// GcFinish - see documentation in gc.h
void GcFinish()
{
  // Run one last collection to verify we don't have any leaks.
  GC_gcollect();
}

// GcArenaAlloc --
//  Implements the alloc method for the GcArena.
//  See the documentation of FblcArena alloc in fblc.h
static void* GcArenaAlloc(FblcArena* this, size_t size)
{
  return GC_MALLOC(size);
}

// GcArenaFree --
//  Implements the free method for the GcArena.
//  See the documentation of FblcArena free in fblc.h
static void GcArenaFree(FblcArena* this, void* ptr)
{
  GC_FREE(ptr);
}

// CreateGcArena -- see documentation in gc.h
FblcArena* CreateGcArena()
{
  FblcArena* arena = GC_MALLOC(sizeof(FblcArena));
  arena->alloc = &GcArenaAlloc;
  arena->free = &GcArenaFree;
  return arena;
}

// FreeGcArena -- see documentation in gc.h
void FreeGcArena(FblcArena* arena)
{
  GC_FREE(arena);
}

// BulkFreeArenaAlloc --
//  Implements the alloc method for the BulkFreeArena.
//  See the documentation of FblcArena alloc in fblc.h
static void* BulkFreeArenaAlloc(FblcArena* this, size_t size)
{
  BulkFreeArena* arena = (BulkFreeArena*)this;
  size += sizeof(BulkFreeArenaAllocList);
  BulkFreeArenaAllocList* alloc = arena->arena->alloc(arena->arena, size);
  alloc->next = arena->allocs;
  arena->allocs = alloc;
  return alloc->data;
}

// BulkFreeArenaFree --
//  Implements the free method for the BulkFreeArena.
//  See the documentation of FblcArena free in fblc.h
static void BulkFreeArenaFree(FblcArena* this, void* ptr)
{
  // Ignored. Allocations will be freed in bulk.
}

// CreateBulkFreeArena -- see documentation in gc.h
FblcArena* CreateBulkFreeArena(FblcArena* arena)
{
  BulkFreeArena* bulk_free_arena = arena->alloc(arena, sizeof(BulkFreeArena));
  bulk_free_arena->alloc = &BulkFreeArenaAlloc;
  bulk_free_arena->free = &BulkFreeArenaFree;
  bulk_free_arena->arena = arena;
  bulk_free_arena->allocs = NULL;
  return (FblcArena*)bulk_free_arena;
}

// FreeBulkFreeArena -- see documentation in gc.h
void FreeBulkFreeArena(FblcArena* arena)
{
  BulkFreeArena* bulk_free_arena = (BulkFreeArena*)arena;
  while (bulk_free_arena->allocs != NULL) {
    void* ptr = bulk_free_arena->allocs;
    bulk_free_arena->allocs = bulk_free_arena->allocs->next;
    bulk_free_arena->arena->free(bulk_free_arena->arena, ptr);
  }
  bulk_free_arena->arena->free(bulk_free_arena->arena, bulk_free_arena);
}
