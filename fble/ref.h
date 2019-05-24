// ref.h --
//   Header file describing the interface for working with fble references.
//   This is an internal library interface.

#ifndef FBLE_REF_H_
#define FBLE_REF_H_

#include <stdbool.h>    // for bool

#include "fble-alloc.h"
#include "fble-vector.h"

// FbleRef --
//   A reference to an object that will be automatically freed when no longer
//   accessible. Extend FbleRef to create a automatically memory managed data
//   type.
//
//   All fields of FbleRef are for internal use only.
typedef struct FbleRef {
  size_t id;
  size_t refcount;
  struct FbleRef* cycle;
} FbleRef;

// FbleRefV --
//   A vector of FbleRefs.
typedef struct {
  size_t size;
  FbleRef** xs;
} FbleRefV;

// FbleRefArena --
//   An arena used for allocating automatically memory managed allocations.
typedef struct FbleRefArena FbleRefArena;

// FbleRefCallback --
//   A call back function used for communicating information about references.
typedef struct FbleRefCallback {
  void (*callback)(struct FbleRefCallback* this, FbleRef* ref);
} FbleRefCallback;

// FbleNewRefArena --
//   Create a new reference arena.
//
// Inputs:
//   arena - The underlying arena to use for allocations
//
//   free --
//       Free the object associated with the given ref, because the ref is no
//       longer acessible.
//    
//     Inputs:
//       arena - the reference arena.
//       ref - the references to free.
//    
//     Results:
//       None.
//    
//     Side effects:
//       Unspecified.
//
//   added --
//       Return the list of references that have been added to the given ref.
//    
//     Inputs:
//       add - callback to call for each added reference.
//       ref - the reference to get the list of added for.
//    
//     Results:
//       None.
//    
//     Side effects:
//       Calls add for every reference x for which FbleRefAdd(arena, ref, x)
//       has been called.
//
//  Results:
//    The newly allocated arena.
//
//  Side effects:
//    Allocates a new reference arena. The caller is resposible for calling
//    FbleDeleteRefArena when the reference arena is no longer needed.
FbleRefArena* FbleNewRefArena(
    FbleArena* arena, 
    void (*free)(FbleRefArena* arena, FbleRef* ref),
    void (*added)(FbleRefCallback* add, FbleRef* ref));

// FbleDeleteRefArena --
//   Delete a reference arena no longer in use.
//
// Inputs:
//   arena - the arena to delete.
//
// Results:
//   none.
//
// Side effects:
//   Frees resources associated with the given arena.
void FbleDeleteRefArena(FbleRefArena* arena);

// FbleRefArenaArena --
//   Returns the arena underlying this ref arena.
//
// Inputs:
//   arena - the ref arena.
//
// Results:
//   The underlying arena of this ref arena.
//
// Side effects:
//   None.
FbleArena* FbleRefArenaArena(FbleRefArena* arena);

// FbleRefInit --
//   Initialize and retain the reference pointed to by ref.
//
// Inputs:
//   arena - The reference arena the ref should belong to.
//   ref - The reference to initialize.
//
// Results:
//   None.
//
// Side effects:
//   Initializes the reference and performs an FbleRefRetain on the reference.
void FbleRefInit(FbleRefArena* arena, FbleRef* ref);

// FbleRefRetain --
//   Cause ref, and any other references that are referred to directly or
//   indirectly from ref, to be retained until a corresponding FbleRefRelease
//   call is made.
//
// Inputs:
//   arena - The reference arena the ref belongs to.
//   ref - The reference to retain.
//
// Results:
//   None.
//
// Side effects:
//   The ref object is retained until a corresponding FbleRefRelease call is
//   made.
void FbleRefRetain(FbleRefArena* arena, FbleRef* ref);

// FbleRefRelease --
//   Release the given reference, cause the reference to be freed if there are
//   no outstanding references to it.
//
// Inputs:
//   arena - The reference arena the ref belongs to.
//   ref - The reference to release.
//
// Results:
//   None.
//
// Side effects:
//   The ref object is released. If there are no more references to it, the
//   ref object is freed.
void FbleRefRelease(FbleRefArena* arena, FbleRef* ref);

// FbleRefAdd --
//   Add a reference from src to dst.
//
// Inputs:
//   arena - the reference arena.
//   src - the source node.
//   dst - the destination node.
//
// Results:
//   None.
//
// Side effects:
//   Adds a reference from the src node to the dst node, so that dst is
//   retained at least as long as src is retained.
//
// TODO: Clarify whether 'added' should include this reference from src to dst
// at the time of this call or not. I think the current implementation assumes
// 'added' does include the reference.
void FbleRefAdd(FbleRefArena* arena, FbleRef* src, FbleRef* dst);

#endif // FBLE_REF_H_
