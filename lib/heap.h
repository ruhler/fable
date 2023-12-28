/**
 * @file heap.h
 *  Defines FbleHeap, the fble garbage collector API.
 */

#ifndef FBLE_INTERNAL_HEAP_H_
#define FBLE_INTERNAL_HEAP_H_

#include <sys/types.h>  // for size_t

/**
 * Heap of objects managed by the garbage collector.
 *
 * Objects are allocated on a heap. They can have references to other objects
 * on the heap, potentially involving cycles.
 *
 * Heaps can be customized by the type of object allocated on the heap. See
 * documentation of FbleNewHeap for more details.
 */
typedef struct FbleHeap FbleHeap;

/**
 * Creates a new garbage collected heap.
 *
 * @return
 *   The newly allocated heap.
 *
 * @sideeffects
 *   Allocates a new heap. The caller is resposible for
 *   calling FbleFreeHeap when the heap is no longer needed.
 */
FbleHeap* FbleNewHeap(
    /**
     * User supplied function to traverse over the objects referenced by obj.
     *
     * This function will be called by the garbage collector as needed to
     * traverse objects on the heap.
     *
     *   
     * param heap This heap.
     * param obj  The object whose references to traverse
     *   
     * sideeffects
     *   The implementation of this function should call FbleHeapAddRef on
     *   each object referenced by obj. If the same object is referenced
     *   multiple times by obj, the callback is called once for each time the
     *   object is referenced by obj.
     */
    void (*refs)(FbleHeap* heap, void* obj),

    /**
     * User supplied function called when an object is freed.
     *
     * This function will be called by the garbage collector after the
     * collector has determined it is done with the object, just before
     * freeing the underlying memory for the object.
     * 
     * param heap  This heap.
     * param obj  The object being freed.
     * 
     * sideeffects
     *   Frees resources outside of the heap that this obj holds on to.
     */
    void (*on_free)(FbleHeap* heap, void* obj));

/**
 * @func[FbleFreeHeap] Frees a heap that is no longer in use.
 *  @arg[FbleHeap*][heap] The heap to free.
 *
 *  @sideeffects
 *   @item
 *    Does a full GC to reclaim all unreachable objects, and frees resources
 *    associated with the given heap.
 *   @item
 *    Does not free objects that are still being retained on the heap. Those
 *    allocations will be leaked.
 */
void FbleFreeHeap(FbleHeap* heap);

/**
 * @func[FbleNewHeapObject] Allocates a new object on the heap.
 *  Callers must ensure the heap is in a consistent state when calling this
 *  function. In particular, calls to the refs function could be made for any
 *  objects previously allocated on the heap, so objects must be fully
 *  initialized.
 *
 *  @arg[FbleHeap*][heap] The heap to allocate an object on.
 *  @arg[size_t][size] The user size of object to allocate.
 *
 *  @returns[void*] A pointer to a newly allocated object on the heap.
 *
 *  @sideeffects
 *   @item
 *    The returned object is retained. A corresponding call to
 *    FbleReleaseHeapObject is required before the object can be freed.
 *   @item
 *    Calls the user provided 'refs' function arbitrarily on existing objects
 *    of the heap.
 */
void* FbleNewHeapObject(FbleHeap* heap, size_t size);

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
void FbleRetainHeapObject(FbleHeap* heap, void* obj);

/**
 * Releases a retained heap object.
 *
 * Releases the given object, allowing the object to be freed if there is
 * nothing else causing it to be retained.
 *
 * @param heap  The heap the object is allocated on.
 * @param obj  The object to release.
 *
 * @sideeffects
 *   The object is released. If there are no more references to it, the
 *   object will (eventually) be freed.
 */
void FbleReleaseHeapObject(FbleHeap* heap, void* obj);

/**
 * @func[FbleHeapObjectAddRef] Adds a reference from one object to another.
 *  Notifies the garbage collector of a reference from src to dst. The dst
 *  object should be included in the refs callback for the src object when
 *  add_ref is called.
 *
 *  @arg[FbleHeap*][heap] The heap the objects are allocated on.
 *  @arg[void*][src] The source object.
 *  @arg[void*][dst] The destination object. Must not be NULL.
 *
 * @sideeffects
 *  Causes the dst object to be retained at least as long as the src object is
 *  retained.
 */
void FbleHeapObjectAddRef(FbleHeap* heap, void* src, void* dst);

/**
 * Does a full GC.
 *
 * Causes the garbage collector to perform a full garbage collection,
 * collecting all objects that are currently unreachable.
 *
 * Full GC can be a very expensive operation. This method is primarily
 * intended to be used to help in testing and debugging of memory use.
 *
 * @param heap  The heap to perform GC on.
 *
 * @sideeffects
 * * Causes all currently unreachable objects on the heap to be freed.
 */
void FbleHeapFullGc(FbleHeap* heap);

#endif // FBLE_INTERNAL_HEAP_H_
