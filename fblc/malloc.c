// malloc.c --
//   This file implements the FblcMallocArena.

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf, stderr
#include <stdlib.h>   // for malloc, free, abort

#include "fblc.h"

// FblcDebugAlloc --
//   A non-circular double linked list of allocation records used to track
//   live allocations in the FblcDebugMallocArena.
struct FblcDebugAlloc {
  FblcDebugAlloc* prev;
  FblcDebugAlloc* next;
  const char* msg;
  char data[];
};

static void* MallocAlloc(FblcArena* this, size_t size, const char* msg);
static void MallocFree(FblcArena* this, void* ptr);
static void* DebugMallocAlloc(FblcArena* this, size_t size, const char* msg);
static void DebugMallocFree(FblcArena* this, void* ptr);

// MallocAlloc -- FblcArena alloc function implemented using malloc.
// See fblc.h for documentation about FblcArena alloc functions.
static void* MallocAlloc(FblcArena* this, size_t size, const char* msg)
{
  return malloc(size);
}

// MallocFree -- FblcArena free function implemented using malloc.
// See fblc.h for documentation about FblcArena alloc functions.
static void MallocFree(FblcArena* this, void* ptr)
{
  free(ptr);
}

// DebugMallocAlloc -- Debug FblcArena alloc function implemented using malloc.
// See fblc.h for documentation about FblcArena alloc functions.
static void* DebugMallocAlloc(FblcArena* this, size_t size, const char* msg)
{
  FblcDebugMallocArena* arena = (FblcDebugMallocArena*)this;
  FblcDebugAlloc* alloc = malloc(sizeof(FblcDebugAlloc) + size);
  alloc->prev = NULL;
  alloc->next = arena->head;
  if (alloc->next != NULL) {
    alloc->next->prev = alloc;
  }
  alloc->msg = msg;
  arena->head = alloc;
  return (void*)alloc->data;
}

// DebugMallocFree -- Debug FblcArena free function implemented using malloc.
// See fblc.h for documentation about FblcArena alloc functions.
static void DebugMallocFree(FblcArena* this, void* ptr)
{
  FblcDebugMallocArena* arena = (FblcDebugMallocArena*)this;

  FblcDebugAlloc* alloc = ((FblcDebugAlloc*)ptr) - 1;
  assert(ptr == (void*)alloc->data);

  if (alloc->prev == NULL) {
    arena->head = alloc->next;
    if (arena->head != NULL) {
      arena->head->prev = NULL;
    }
  } else {
    alloc->prev->next = alloc->next;
    if (alloc->next != NULL) {
      alloc->next->prev = alloc->prev;
    }
  }

  free(alloc);
}

// FblcMallocArena -- see documentation in fblc.h
FblcArena FblcMallocArena = { .alloc = &MallocAlloc, .free = &MallocFree };

// FblcInitDebugMallocArena -- see documentation in fblc.h
void FblcInitDebugMallocArena(FblcDebugMallocArena* arena) {
  arena->_base.alloc = &DebugMallocAlloc;
  arena->_base.free = &DebugMallocFree;
  arena->head = NULL;
}

void FblcAssertEmptyDebugMallocArena(FblcDebugMallocArena* arena) {
  if (arena->head != NULL) {
    fprintf(stderr, "the following allocations are outstanding:\n");
    for (FblcDebugAlloc* alloc = arena->head; alloc != NULL; alloc = alloc->next) {
      fprintf(stderr, "%s\n", alloc->msg);
    }
    abort();
  }
}
