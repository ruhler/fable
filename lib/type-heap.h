/**
 * @field type-heap.h
 *  Internal API for communication between type-heap.c and type.c.
 */

#ifndef FBLE_INTERNAL_TYPE_HEAP_H_
#define FBLE_INTERNAL_TYPE_HEAP_H_

#include "type.h"

/**
 * @func[FbleNewHeapObject] Allocates a new object on the heap.
 *  Callers must ensure the heap is in a consistent state when calling this
 *  function. In particular, calls to the refs function could be made for any
 *  objects previously allocated on the heap, so objects must be fully
 *  initialized.
 *
 *  @arg[FbleTypeHeap*][heap] The heap to allocate an object on.
 *  @arg[size_t][size] The user size of object to allocate.
 *
 *  @returns[FbleType*] A pointer to a newly allocated object on the heap.
 *
 *  @sideeffects
 *   @item
 *    The returned object is retained. A corresponding call to
 *    FbleReleaseHeapObject is required before the object can be freed.
 *   @item
 *    Calls the user provided 'refs' function arbitrarily on existing objects
 *    of the heap.
 */
FbleType* FbleNewHeapObject(FbleTypeHeap* heap, size_t size);

/**
 * Retains an object on the heap.
 *
 * Causes obj, and any other references that are referred to directly or
 * indirectly from obj, to be retained until a corresponding call to
 * FbleReleaseHeapObject is made.
 *
 * @param heap  The heap the object is allocated on.
 * @param obj  The object to retain.
 *
 * @sideeffects
 *   The object is retained until a corresponding FbleReleaseHeapObject is
 *   made.
 */
void FbleRetainHeapObject(FbleTypeHeap* heap, FbleType* obj);

/**
 * @func[FbleReleaseHeapObject] Releases a retained heap object.
 *  Releases the given object, allowing the object to be freed if there is
 *  nothing else causing it to be retained.
 *
 *  @arg[FbleTypeHeap*][heap] The heap the object is allocated on.
 *  @arg[FbleTypeHeap*][obj] The object to release.
 *
 *  @sideeffects
 *   The object is released. If there are no more references to it, the
 *   object will (eventually) be freed.
 */
void FbleReleaseHeapObject(FbleTypeHeap* heap, FbleType* obj);

/**
 * @func[FbleHeapObjectAddRef] Adds a reference from one object to another.
 *  Notifies the garbage collector of a reference from src to dst. The dst
 *  object should be included in the refs callback for the src object when
 *  add_ref is called.
 *
 *  @arg[FbleTypeHeap*][heap] The heap the objects are allocated on.
 *  @arg[FbleType*][src] The source object.
 *  @arg[FbleType*][dst] The destination object. Must not be NULL.
 *
 *  @sideeffects
 *   Causes the dst object to be retained at least as long as the src object is
 *   retained.
 */
void FbleHeapObjectAddRef(FbleTypeHeap* heap, FbleType* src, FbleType* dst);

/**
 * @func[FbleTypeRefs]
 *  Function to traverse over the objects referenced by obj.
 *
 *  This function will be called by the garbage collector as needed to
 *  traverse objects on the heap.
 *
 *  @arg[FbleTypeHeap*][heap] This heap.
 *  @arg[FbleType*][obj] The object whose references to traverse
 *   
 *  @sideeffects
 *   The implementation of this function calls FbleHeapAddRef on
 *   each object referenced by obj. If the same object is referenced
 *   multiple times by obj, the callback is called once for each time the
 *   object is referenced by obj.
 */
void FbleTypeRefs(FbleTypeHeap* heap, FbleType* obj);

/**
 * @func[FbleTypeOnFree] Function called when a type object is freed.
 *  This function is called by the garbage collector after the
 *  collector has determined it is done with the object, just before
 *  freeing the underlying memory for the object.
 * 
 *  @arg[FbleTypeHeap*][heap] This heap.
 *  @arg[FbleType*][obj] The object being freed.
 * 
 *  @sideeffects
 *   Frees resources outside of the heap that this obj holds on to.
 */
void FbleTypeOnFree(FbleType* obj);


#endif // FBLE_INTERNAL_TYPE_HEAP_H_
