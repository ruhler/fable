/**
 * @file heap.h
 * Defines FbleHeap, the fble garbage collector API.
 */

#ifndef FBLE_INTERNAL_HEAP_H_
#define FBLE_INTERNAL_HEAP_H_

#include <sys/types.h>  // for size_t

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
//   A heap of objects that are managed by a garbage collector.
//
// Objects are allocated on a heap. They can have references to other objects
// on the heap, potentially involving cycles.
//
// Heaps can be customized by the type of object allocated on the heap. See
// documentation of FbleNewHeap for more details.
typedef struct FbleHeap FbleHeap;

// FbleNewHeap --
//   Create a new heap.
//
// Inputs:
//   refs -
//     User supplied function to traverse over the objects referenced by obj.
//
//     This function will be called by the garbage collector as needed to
//     traverse objects on the heap.
//     
//     Inputs:
//       callback - callback to call for each object referenced by obj
//       obj - the object whose references to traverse
//     
//     Side effects:
//       Calls the callback function for each object referenced by obj. If the
//       same object is referenced multiple times by obj, the callback is
//       called once for each time the object is referenced by obj.
//
//   on_free --
//     User supplied function called when an object is freed.
//
//     This function will be called by the garbage collector after the
//     collector has determined it is done with the object, just before
//     freeing the underlying memory for the object.
//     
//     Inputs:
//       heap - the heap.
//       obj - the object being freed.
//     
//     Side effects:
//       Intended side effects are to free any resources outside of the heap
//       that this obj holds on to.
//
//  Results:
//    The newly allocated heap.
//
//  Side effects:
//    Allocates a new heap. The caller is resposible for
//    calling FbleFreeHeap when the heap is no longer needed.
FbleHeap* FbleNewHeap(
    void (*refs)(FbleHeapCallback*, void*),
    void (*on_free)(FbleHeap*, void*));

// FbleFreeHeap --
//   Free a heap that is no longer in use.
//
// Inputs:
//   heap - the heap to free.
//
// Side effects:
//   Does a full GC to reclaim all unreachable objects, and frees resources
//   associated with the given heap.
//
//   Does not free objects that are still being retained on the heap. Those
//   allocations will be leaked.
void FbleFreeHeap(FbleHeap* heap);

// FbleNewHeapObject --
//   Allocate a new object on the heap.
//
// Callers must ensure the heap is in a consistent state when calling this
// function. In particular, calls to the refs function could be made for any
// objects previously allocated on the heap, so objects must be fully
// initialized.
//
// Inputs:
//   heap - the heap to allocate an object on.
//   size - the user size of object to allocate
//
// Result:
//   A pointer to a newly allocated object on the heap.
//
// Side effects:
// * The returned object is retained. A corresponding call to
//   FbleReleaseHeapObject is required before the object can be freed.
// * Calls the user provided 'refs' function arbitrarily on existing objects
//   of the heap.
void* FbleNewHeapObject(FbleHeap* heap, size_t size);

// FbleRetainHeapObject --
//   Cause obj, and any other references that are referred to directly or
//   indirectly from obj, to be retained until a corresponding call to
//   FbleReleaseHeapObject is made.
//
// Inputs:
//   heap - the heap the object is allocated on.
//   obj - The object to retain.
//
// Side effects:
//   The object is retained until a corresponding FbleReleaseHeapObject is
//   made.
void FbleRetainHeapObject(FbleHeap* heap, void* obj);

// FbleReleaseHeapObject --
//   Release the given object, allowing the object to be freed if there is
//   nothing else causing it to be retained.
//
// Inputs:
//   heap - the heap the object is allocated on.
//   obj - The object to release.
//
// Side effects:
//   The object is released. If there are no more references to it, the
//   object will (eventually) be freed.
void FbleReleaseHeapObject(FbleHeap* heap, void* obj);

// FbleHeapObjectAddRef --
//   Notify the garbage collector that a reference has been added from src
//   to dst. The dst object should be included in the refs callback for the
//   src object when add_ref is called.
//
// Inputs:
//   heap - the heap the objects are allocated on.
//   src - the source object.
//   dst - the destination object. Must not be NULL.
//
// Side effects:
//   Causes the dst object to be retained at least as long as the src object
//   is retained.
void FbleHeapObjectAddRef(FbleHeap* heap, void* src, void* dst);

// FbleHeapFullGc --
//   Causes the garbage collector to perform a full garbage collection,
//   collecting all objects that are currently unreachable.
//
// Full GC can be a very expensive operation. This method is primarily
// intended to be used to help in testing and debugging of memory use.
//
// Inputs:
//   heap - the heap to perform GC on.
//
// Side effects:
//   Causes all currently unreachable objects on the heap to be freed.
void FbleHeapFullGc(FbleHeap* heap);

#endif // FBLE_INTERNAL_HEAP_H_
