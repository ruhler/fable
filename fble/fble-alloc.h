// fble-arena.h --
//   Header file describing arena base allocation functionality for fble.

#ifndef FBLE_ALLOC_H_
#define FBLE_ALLOC_H_

#include <sys/types.h>    // for size_t

// FbleArena --
//   A handle used for allocating and freeing memory.
typedef struct FbleArena FbleArena;

// FbleArenaAlloc --
//   Allocate size bytes of memory.
//
// Inputs:
//   arena - The arena to allocate memory from.
//   size - The number of bytes to allocate.
//   msg - A message used to identify the allocation for debugging purposes.
//
// Result:
//   A pointer to a newly allocated size bytes of memory in the arena.
//
// Side effects:
//   Allocates size bytes from the arena.
void* FbleArenaAlloc(FbleArena* arena, size_t size, const char* msg);

// FbleAlloc --
//   A type safer way of allocating objects from an arena.
//
// Inputs:
//   arena - The arena to use for allocation.
//   T - The type of object to allocate.
//
// Results:
//   A pointer to a newly allocated object of the given type.
//
// Side effects:
//   Uses the arena to allocation the object.
#define FbleAllocLine(x) #x
#define FbleAllocMsg(file, line) file ":" FbleAllocLine(line)
#define FbleAlloc(arena, T) ((T*) FbleArenaAlloc(arena, sizeof(T), FbleAllocMsg(__FILE__, __LINE__)))

// FbleFree --
//   Free a memory allocation.
//
// Inputs:
//   arena - The arena to free the memory from.
//   ptr - The allocation to free, which must have been returned by a call
//         to alloc on this arena. ptr may be NULL.
//
// Result:
//   None.
//
// Side effects:
//   Frees memory associated with the ptr from the arena.
//   After this call, any accesses to the freed memory result in undefined
//   behavior.
//   Behavior is undefined if ptr was not previously returned by a call to
//   alloc on the given arena.
void FbleFree(FbleArena* arena, void* ptr);

// FbleNewArena --
//   Create a new arena descended from the given arena.
//
// Inputs:
//   parent - the parent arena for the new arena. May be null.
//
// Result:
//   The newly allocated arena.
//
// Side effects:
//   Allocates a new arena which must be freed using FbleDeleteArena.
FbleArena* FbleNewArena(FbleArena* parent);

// FbleDeleteArena --
//   Delete an arena created with FbleNewArena.
//
// Inputs:
//   arena - the arena to delete.
//
// Result:
//   None.
//
// Side effects:
//   Frees memory associated with the arena, including the arena itself, all
//   outstanding allocations made by the arena, and all descendant arenas and
//   their allocations.
void FbleDeleteArena(FbleArena* arena);

// FbleAssertEmptyArena
//   Check that there are no outstanding allocations in the given arena. Used
//   to aid in testing and debugging memory leaks.
//
// Inputs:
//   arena - A pointer to the arena to check.
//
// Results:
//   none.
//
// Side effects:
//   Prints debug message to stderr and aborts the process if there are any
//   outstanding allocations in the arena.
void FbleAssertEmptyArena(FbleArena* arena);

// FbleArenaMaxSize
//   Returns the maximum size the given arena reached in bytes.
//
// Inputs:
//   arena - The arena to get the max size of.
//
// Results:
//   The maximum size of the arena.
//
// Side effects:
//   None.
size_t FbleArenaMaxSize(FbleArena* arena);

#endif // FBLE_ALLOC_H_
