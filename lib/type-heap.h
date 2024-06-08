/**
 * @field type-heap.h
 *  Internal API for communication between type-heap.c and type.c.
 */

#ifndef FBLE_INTERNAL_TYPE_HEAP_H_
#define FBLE_INTERNAL_TYPE_HEAP_H_

#include "type.h"

/**
 * @func[FbleAllocType] Allocates a new object on the heap.
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
FbleType* FbleAllocType(FbleTypeHeap* heap, size_t size);

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
 *   The implementation of this function calls FbleTypeAddRef on
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
