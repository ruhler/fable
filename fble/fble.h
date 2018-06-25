// fble.h --
//   This header file describes the externally visible interface to the
//   fble library.

#ifndef FBLE_H_
#define FBLE_H_

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
//         to alloc on this arena.
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

// FbleVector --
//   A common data structure in fble is an array of elements with a size. By
//   convention, fble uses the same data structure layout and naming for all
//   of these vector data structures. The type of a vector of elements T is:
//   struct {
//     size_t size;
//     T* xs;
//   };
//
//   Often these vectors are constructed without knowing the size ahead of
//   time. The following macros are used to help construct these vectors,
//   regardless of the element type.

// FbleVectorInit --
//   Initialize a vector for construction.
//
// Inputs:
//   arena - The arena to use for allocations.
//   vector - A reference to an uninitialized vector.
//
// Results:
//   None.
//
// Side effects:
//   The vector is initialized to an array containing 0 elements.
//
// Implementation notes:
//   The array initially has size 0 and capacity 1.
#define FbleVectorInit(arena, vector) \
  (vector).size = 0; \
  (vector).xs = FbleArenaAlloc(arena, sizeof(*((vector).xs)), FBLC_ALLOC_MSG(__FILE__, __LINE__))

// FbleVectorExtend --
//   Append an uninitialized element to the vector.
//
// Inputs:
//   arena - The arena to use for allocations.
//   vector - A reference to a vector that was initialized using FbleVectorInit.
//
// Results:
//   A pointer to the newly appended uninitialized element.
//
// Side effects:
//   A new uninitialized element is appended to the array and the size is
//   incremented. If necessary, the array is re-allocated to make space for
//   the new element.
#define FbleVectorExtend(arena, vector) \
  (FbleVectorIncrSize(arena, sizeof(*((vector).xs)), &(vector).size, (void**)&(vector).xs), (vector).xs + (vector).size - 1)

// FbleVectorAppend --
//   Append an element to a vector.
//
// Inputs:
//   arena - The arena to use for allocations.
//   vector - A reference to a vector that was initialized using FbleVectorInit.
//   elem - An element of type T to append to the array.
//
// Results:
//   None.
//
// Side effects:
//   The given element is appended to the array and the size is incremented.
//   If necessary, the array is re-allocated to make space for the new
//   element.
#define FbleVectorAppend(arena, vector, elem) \
  (*FbleVectorExtend(arena, vector) = elem)

// FbleVectorIncrSize --
//   Increase the size of an fble vector by a single element.
//
//   This is an internal function used for implementing the fble vector macros.
//   This function should not be called directly because because it does not
//   provide the same level of type safety the macros provide.
//
// Inputs:
//   arena - The arena used for allocations.
//   elem_size - The sizeof the element type in bytes.
//   size - A pointer to the size field of the vector.
//   xs - A pointer to the xs field of the vector.
//
// Results:
//   None.
//
// Side effects:
//   A new uninitialized element is appended to the vector and the size is
//   incremented. If necessary, the array is re-allocated to make space for
//   the new element.
void FbleVectorIncrSize(FbleArena* arena, size_t elem_size, size_t* size, void** xs);


#endif // FBLE_H_
