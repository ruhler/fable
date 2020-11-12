// heap.h --
//   Header file for the internal fble heap API.

#ifndef FBLE_INTERNAL_HEAP_H_
#define FBLE_INTERNAL_HEAP_H_

#include "fble-alloc.h"   // for FbleArena

// FbleHeapCallback --
//   A callback function used when traversing objects on a heap.
//
// This is intended to be used as a base class for custom callback functions
// that require additional user arguments.
typedef struct FbleHeapCallback {
  // callback --
  //   Generic callback function.
  //
  // Inputs:
  //   this - This FbleHeapCallback instance.
  //   obj - The heap object being visited in the traversal.
  //
  // Results:
  //   None.
  //
  // Side effects:
  //   Anything that will not interfer with the traversal operation.
  void (*callback)(struct FbleHeapCallback* this, void* obj);
} FbleHeapCallback;

// FbleHeap --
//   A heap of objects.
//
// Objects are allocated on a heap. They can have references to other objects
// on the heap, potentially involving cycles.
//
// Heaps can be customized in two dimensions: by the value type, and by the
// garbage collection implementation.
//
// It is strongly discouraged for users to access the functions directly from
// the FbleHeap structure. Instead wrapper functions should be provided for
// the value type that provide type safety you don't get when passing void*
// arguments directly.
typedef struct FbleHeap {
  // arena -- arena to use for underlying allocations.
  FbleArena* arena;

  // TODO: add a roots function to traverse over roots if/when desired?
  // TODO: add back the old del_ref function if/when desired to notify the gc
  // when we remove a reference?

  // refs --
  //   Traverse over the objects referenced by obj.
  //
  // This function should be supplied by the implementation of the object
  // type. It will be called by the garbage collection implementation.
  //
  // Inputs:
  //   callback - callback to call for each object referenced by obj
  //   obj - the object whose references to traverse
  //
  // Side effects:
  //   Calls the callback function for each object referenced by obj. If the
  //   same object is referenced multiple times by obj, the callback is
  //   called once for each time the object is referenced by obj.
  void (*refs)(FbleHeapCallback* callback, void* obj);

  // on_free --
  //   Function called when an object is freed.
  //
  // This function should be supplied by the implementation of the object
  // type. It will be called by the garbage collection implementation after
  // the gc has determined it is done with the object. The gc will free the
  // underlying memory for the object after this call.
  //
  // Inputs:
  //   heap - the heap.
  //   obj - the object being freed.
  //
  // Side effects:
  //   Intended side effects are to free any resources outside of the heap
  //   that this obj holds on to.
  void (*on_free)(struct FbleHeap* heap, void* obj);

  // new --
  //   Allocate a new object on the heap.
  //
  // This function should be supplied by the implementation of the garbage
  // collector. It will be called by the user of the heap.
  //
  // Callers must ensure the heap is in a consistent state when calling this
  // function. In particular, calls to the refs function could be made for any
  // objects previously allocated on the heap, so they must be fully
  // initialized.
  //
  // Inputs:
  //   heap - this heap
  //   size - the user size of object to allocate
  //
  // Result:
  //   A pointer to a newly allocated object on the heap.
  //
  // Side effects:
  //   * The returned object is retained. A corresponding call to release is
  //     required before the object can be freed.
  //   * Calls the 'refs' function arbitrarily on existing objects of the
  //     heap.
  void* (*new)(struct FbleHeap* heap, size_t size);

  // retain --
  //   Cause obj, and any other references that are referred to directly or
  //   indirectly from obj, to be retained until a corresponding release
  //   call is made.
  //
  // This function should be supplied by the implementation of the garbage
  // collector. It will be called by the user of the heap.
  //
  // Inputs:
  //   heap - this heap
  //   obj - The object to retain.
  //
  // Side effects:
  //   The object is retained until a corresponding release call is made.
  void (*retain)(struct FbleHeap* heap, void* obj);

  // release --
  //   Release the given object, allowing the object to be freed if there is
  //   nothing else causing it to be retained.
  //
  // This function should be supplied by the implementation of the garbage
  // collector. It will be called by the user of the heap.
  //
  // Inputs:
  //   heap - this heap
  //   obj - The object to release.
  //
  // Side effects:
  //   The object is released. If there are no more references to it, the
  //   object will (eventually) be freed.
  void (*release)(struct FbleHeap* heap, void* obj);

  // add_ref --
  //   Notify the garbage collector that a reference has been added from src
  //   to dst. The dst object should be included in the refs callback for the
  //   src object when add_ref is called.
  //
  // This function should be supplied by the implementation of the garbage
  // collector. It will be called by the user of the heap.
  //
  // Inputs:
  //   heap - this heap
  //   src - the source object.
  //   dst - the destination object. Must not be NULL.
  //
  // Side effects:
  //   Causes the dst object to be retained at least as long as the src object
  //   is retained.
  void (*add_ref)(struct FbleHeap* heap, void* src, void* dst);

  // full_gc --
  //   Causes the garbage collector to perform a full garbage collection,
  //   collecting all objects that are currently unreachable.
  //
  // Full GC can be a very expensive operation. This method is primarily
  // intended to be used to help in testing and debugging of memory use.
  //
  // Inputs:
  //   heap - this heap.
  //
  // Side effects:
  //   Causes all currently unreachable objects on the heap to be freed.
  void (*full_gc)(struct FbleHeap* heap);
} FbleHeap;

// FbleNewMarkSweepHeap --
//   Create a new mark sweep based heap.
//
// Inputs:
//   arena - The underlying arena to use for allocations
//   refs - The refs function associated with the object type.
//   on_free - The on_free function associated with the object type.
//
//  Results:
//    The newly allocated heap.
//
//  Side effects:
//    Allocates a new mark sweep heap. The caller is resposible for
//    calling FbleFreeMarkSweepHeap when the heap is no longer needed.
FbleHeap* FbleNewMarkSweepHeap(
    FbleArena* arena, 
    void (*refs)(FbleHeapCallback*, void*),
    void (*on_free)(FbleHeap*, void*));

// FbleFreeMarkSweepHeap --
//   Free a mark sweep heap that is no longer in use.
//
// Inputs:
//   heap - the heap to delete.
//
// Results:
//   none.
//
// Side effects:
//   Does a full GC to reclaim all unreachable objects, and frees resources
//   associated with the given heap.
//
//   Does not free objects that are still being retained on the heap. Those
//   will be leaked.
void FbleFreeMarkSweepHeap(FbleHeap* heap);

#endif // FBLE_INTERNAL_HEAP_H_
