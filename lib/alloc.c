/**
 * @file alloc.c
 * Implementation of fble allocation routines.
 */

#define _GNU_SOURCE   // for MAP_ANONYMOUS

#include <fble/fble-alloc.h>

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf, stderr
#include <stdbool.h>  // for bool
#include <stdlib.h>   // for malloc
#include <sys/mman.h> // for mmap, munmap
#include <unistd.h>   // for sysconf


/** Total number of bytes currently allocated via FbleAlloc routines. */
size_t gTotalBytesAllocated = 0;

/**
 * The maximum value of gTotalBytesAllocated since gMaxTotalBytesAllocated
 * was last cleared.
 */
size_t gMaxTotalBytesAllocated = 0;

/** True if the exit routine has been registered. */
bool gInitialized = false;

/**
 * An allocation.
 */
typedef struct {
  size_t size;      /**< Size of the allocation. */
  char data[];      /**< Allocated memory region available to the user. */
} Alloc;

/**
 * Block of memory allocated for use in stack allocations.
 */
typedef struct StackChunk {
  /** The next chunk down in the stack. */
  struct StackChunk* tail;    

  /** Pointer into data for where next allocation should come from. */
  char* top;        
  char* end;      /**< The end boundary of the data. */
  char data[];    /**< The raw data region used for allocations. */
} StackChunk;

/**
 * A stack allocator.
 */
struct FbleStackAllocator {
  /** The current chunk to use for stack allocations. */
  StackChunk* current;  

  /** Optional cached next chunk to use for stack allocations. */
  StackChunk* next;     
};

static void CountAlloc(size_t size);
static void CountFree(size_t size);
static void Exit();

/**
 * Updates allocation metrics for a new allocation.
 *
 * @param size  The size of the new allocation.
 *
 * @sideeffects
 * * Initializes the allocation atExit routine if appropriate.
 * * Updates gTotalBytesAllocated and gMaxTotalBytesAllocated.
 */
static void CountAlloc(size_t size)
{
  if (!gInitialized) {
    gInitialized = true;
    atexit(&Exit);
  }

  gTotalBytesAllocated += size;
  if (gTotalBytesAllocated > gMaxTotalBytesAllocated) {
    gMaxTotalBytesAllocated = gTotalBytesAllocated;
  }
}

/**
 * Updates allocation tracking for a freed allocation.
 *
 * @param size  The size of the freed allocation.
 *
 * @sideeffects
 * * Updates gTotalBytesAllocated.
 */
static void CountFree(size_t size)
{
  gTotalBytesAllocated -= size;
}

/**
 * Cleans up the global arena.
 *
 * @sideeffects
 * * Frees resources associated with the global arena.
 * * Checks for memory leaks.
 */
static void Exit()
{
  if (gTotalBytesAllocated != 0) {
    fprintf(stderr, "ERROR: MEMORY LEAK DETECTED\n");
    fprintf(stderr, "Try running again using: valgrind --leak-check=full\n");
    abort();
  }
}

// See documentation in fble-alloc.h.
void* FbleRawAlloc(size_t size)
{
  CountAlloc(size);
  Alloc* alloc = malloc(sizeof(Alloc) + size);
  alloc->size = size;
  return (void*)alloc->data;
}

// See documentation in fble-alloc.h.
void FbleFree(void* ptr)
{
  if (ptr == NULL) {
    return;
  }

  Alloc* alloc = ((Alloc*)ptr) - 1;
  assert(ptr == (void*)alloc->data);
  CountFree(alloc->size);
  free(alloc);
}

// See documentation in fble-alloc.h.
FbleStackAllocator* FbleNewStackAllocator()
{
  FbleStackAllocator* allocator = FbleAlloc(FbleStackAllocator);

  // Initialize the stack allocator with a 1 page chunk of memory.
  size_t page_size = sysconf(_SC_PAGESIZE);
  StackChunk* chunk = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  chunk->tail = NULL;
  chunk->top = chunk->data;
  chunk->end = ((char*)chunk + page_size);

  allocator->current = chunk;
  allocator->next = NULL;

  return allocator;
}

// See documentation in fble-alloc.h.
void FbleFreeStackAllocator(FbleStackAllocator* allocator)
{
  StackChunk* chunk = allocator->current;
  if (allocator->next != NULL) {
    chunk = allocator->next;
  }

  while (chunk != NULL) {
    StackChunk* tail = chunk->tail;
    munmap(chunk, chunk->end - (char*)chunk);
    chunk = tail;
  }

  FbleFree(allocator);
}

// See documentation in fble-alloc.h.
void* FbleRawStackAlloc(FbleStackAllocator* allocator, size_t size)
{
  // Align size to 8 bytes, because I think that's necessary?
  size = (size + 7) & (~7);

  CountAlloc(size);

  // See if we can allocate using the current chunk.
  StackChunk* chunk = allocator->current;
  if (chunk->top + size <= chunk->end) {
    void* alloc = chunk->top;
    chunk->top += size;
    return alloc;
  }

  // See if we can allocate using the next chunk.
  if (allocator->next != NULL) {
    allocator->current = allocator->next;
    allocator->next = NULL;
  }

  // Allocate more chunks as needed to make space for the allocation.
  chunk = allocator->current;
  while (chunk->end < chunk->top + size) {
    size_t length = 2 * (chunk->end - (char*)chunk);
    chunk = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    chunk->tail = allocator->current;
    chunk->top = chunk->data;
    chunk->end = ((char*)chunk + length);
    allocator->current = chunk;
  }

  void* alloc = chunk->top;
  chunk->top += size;
  return alloc;
}

// See documentation in fble-alloc.h.
void FbleStackFree(FbleStackAllocator* allocator, void* ptr)
{
  StackChunk* chunk = allocator->current;
  while ((char*)ptr < (char*)chunk->data || chunk->end <= (char*)ptr) {
    // The pointer must have been allocated in a previous chunk.
    if (allocator->next != NULL) {
      munmap(allocator->next, allocator->next->end - (char*)allocator->next);
    }
    allocator->next = chunk;
    allocator->current = chunk->tail;
    chunk = chunk->tail;
  }

  CountFree(chunk->top - (char*)ptr);
  chunk->top = ptr;
}

// See documentation in fble-alloc.h.
size_t FbleMaxTotalBytesAllocated()
{
  return gMaxTotalBytesAllocated;
}

// See documentation in fble-alloc.h.
void FbleResetMaxTotalBytesAllocated()
{
  gMaxTotalBytesAllocated = gTotalBytesAllocated;
}
