// malloc.c --
//   This file implements the FblcMallocArena.

#include <stdlib.h>   // for malloc, free

#include "fblc.h"

static void* MallocAlloc(FblcArena* this, size_t size);
static void MallocFree(FblcArena* this, void* ptr);

// MallocAlloc -- FblcArena alloc function implemented using malloc.
// See fblc.h for documentation about FblcArena alloc functions.
static void* MallocAlloc(FblcArena* this, size_t size)
{
  return malloc(size);
}

// MallocFree -- FblcArena free function implemented using malloc.
// See fblc.h for documentation about FblcArena alloc functions.
static void MallocFree(FblcArena* this, void* ptr)
{
  free(ptr);
}

// FblcMallocArena -- see documentation in fblc.h
FblcArena FblcMallocArena = { .alloc = &MallocAlloc, .free = &MallocFree };
