// ref.c --
//   This file implements the ref.h apis.

#include <assert.h>   // for assert
#include <stdlib.h>   // for NULL

#include "ref.h"

// FbleRef -- see definition and documentation in ref.h
//
// Fields:
//   id - A unique identifier for the node. Ids are assigned in increasing
//        order of node allocation.
//   refcount - The number of references to this node. 0 if the node
//              is a child node in a cycle.
//   cycle - A pointer to the head of the cycle this node belongs to. NULL
//           if the node is not a child node of a cycle.

struct FbleRefArena {
  FbleArena* arena;
  size_t next_id;
  void (*free)(struct FbleRefArena* arena, FbleRef* ref);
  void (*added)(FbleRefCallback* add, FbleRef* ref);
};

// Hash table entry node.
typedef struct {
  FbleRef* key;
  size_t value;
} Entry;

// Map --
//   A map from references to size_t values.
typedef struct {
  size_t size;
  size_t capacity;
  Entry* entries;
} Map;

typedef struct {
  FbleRefCallback _base;
  FbleArena* arena;
  FbleRefV* refs;
} AddToVectorCallback;

static void InitMap(FbleArena* arena, Map* map, size_t capacity);
static void FreeMap(FbleArena* arena, Map* map);
static size_t Insert(FbleArena* arena, Map* map, FbleRef* key, size_t value);
static bool In(Map* map, FbleRef* key);

static void AddToVector(AddToVectorCallback* add, FbleRef* ref);
static bool Contains(FbleRefV* refs, FbleRef* ref);

static FbleRef* CycleHead(FbleRef* ref);
static void CycleAdded(FbleRefArena* arena, FbleRefCallback* add, FbleRef* ref);

// InitMap --
//   Initialize a reference map.
//
// Inputs:
//   arena - arena to use for allocations.
//   map - a pointer to an uninitialized Map.
//   capacity - the initial capacity to use for the map.
//
// Results: 
//   none
//
// Side effects:
//   Initializes the map. The caller should call FreeMap when done with
//   using the map to release memory allocated for the Map.
static void InitMap(FbleArena* arena, Map* map, size_t capacity)
{
  map->size = 0;
  map->capacity = capacity;
  map->entries = FbleArrayAlloc(arena, Entry, capacity);
  for (size_t i = 0; i < capacity; ++i) {
    map->entries[i].key = NULL;
    map->entries[i].value = 0;
  }
}

// FreeMap --
//   Free the memory resources used for a Map
//
// Inputs:
//   arena - the arena to use for allocations.
//   map - the map to free the memory of.
//
// Results:
//   none.
//
// Side effects:
//   Frees memory associated with the ref map. Do not access the ref map after
//   this call.
static void FreeMap(FbleArena* arena, Map* map)
{
  FbleFree(arena, map->entries);
}

// Insert --
//   Insert an element into the given ref map.
//
// Inputs:
//   arena - the arena to use for allocations.
//   map - the map to insert into.
//   key - the reference to insert.
//   value - the default value to associate with the reference.
//
// Results:
//   The value associated with the reference. If the reference was already in
//   the map, this is the value previously associated with the reference.
//   Otherwise this is the value just supplied.
//
// Side effects:
//   Associates the given value with the reference in the map if the reference
//   was not already in the map. If the reference was already in the map, does
//   nothing. May resize the map, invalidating any pointers into the map entries.
static size_t Insert(FbleArena* arena, Map* map, FbleRef* key, size_t value)
{
  assert(map->capacity > map->size);
  size_t i = (size_t)key % map->capacity;
  while (map->entries[i].key != NULL) {
    if (map->entries[i].key == key) {
      return map->entries[i].value;
    }
    i = (i + 1) % map->capacity;
  }

  map->entries[i].key = key;
  map->entries[i].value = value;
  map->size++;

  if (3 * map->size > map->capacity) {
    // It is time to resize the map.
    Map nmap;
    InitMap(arena, &nmap, 2 * map->capacity - 1);
    for (size_t i = 0; i < map->capacity; ++i) {
      if (map->entries[i].key != NULL) {
        Insert(arena, &nmap, map->entries[i].key, map->entries[i].value);
      }
    }
    FbleFree(arena, map->entries);
    map->capacity = nmap.capacity;
    map->entries = nmap.entries;
  }
  return value;
}

// In --
//   Check whether a reference map contains the given reference.
//
// Inputs:
//   map - the map to check.
//   key - the reference to check for.
//
// Results: 
//   True if key is in map, false otherwise.
//
// Side effects:
//   None.
static bool In(Map* map, FbleRef* key)
{
  assert(map->capacity > map->size);
  size_t i = (size_t)key % map->capacity;
  while (map->entries[i].key != NULL) {
    if (map->entries[i].key == key) {
      return true;
    }
    i = (i + 1) % map->capacity;
  }
  return false;
}

// AddToVector --
//   An FbleRefCallback that appends a ref to the given vector.
//
// Input:
//   add - this callback.
//   ref - the ref to add.
//
// Results:
//   none.
//
// Side effects:
//   Adds the ref to the vector.
static void AddToVector(AddToVectorCallback* add, FbleRef* ref)
{
  FbleVectorAppend(add->arena, *add->refs, ref);
}

// Contains --
//   Check of a vector of references contains a give referece.
//
// Inputs:
//   refs - the vector of references.
//   ref - the reference to check for
//
// Results:
//   true if the vector of references contains the given reference, false
//   otherwise.
//
// Side effects:
//   None.
static bool Contains(FbleRefV* refs, FbleRef* ref)
{
  for (size_t i = 0; i < refs->size; ++i) {
    if (refs->xs[i] == ref) {
      return true;
    }
  }
  return false;
}

// CycleHead --
//   Get the head of the biggest cycle that ref belongs to.
//
// Inputs:
//   ref - The reference to get the head of.
//
// Results:
//   The head of the biggest cycle that ref belongs to.
//
// Side effects:
//   None.
static FbleRef* CycleHead(FbleRef* ref)
{
  while (ref->cycle != NULL) {
    ref = ref->cycle;
  }
  return ref;
}

// CycleAddedChildCallback --
//   Callback used in the implementation of CycleAdded.
typedef struct {
  FbleRefCallback _base;
  FbleArena* arena;
  FbleRef* ref;
  Map visited;
  FbleRefV* stack;
  FbleRefCallback* add;
} CycleAddedChildCallback;

// CycleAddedChild --
//   Called for each child visited in a cycle. If the child is internal, adds
//   it to the stack to process. Otherwise calls the 'add' callback on the
//   child's cycle head.
static void CycleAddedChild(CycleAddedChildCallback* data, FbleRef* child)
{
  if (child == data->ref || CycleHead(child) == data->ref) {
    if (child != data->ref) {
      size_t index = data->visited.size;
      if (index == Insert(data->arena, &data->visited, child, index)) {
        FbleVectorAppend(data->arena, *data->stack, child);
      }
    }
  } else {
    data->add->callback(data->add, CycleHead(child));
  }
}

// CycleAdded --
//   Get the list of added nodes for a cycle.
//
// Inputs:
//   arena - the reference arena.
//   add - callback to call for each reference.
//   ref - the reference to get the list of added for.
//
// Results:
//   None.
//
// Side effects:
//   Calls the add callback on the cycle head of every reference x that is external to
//   the cycle but reachable by direct reference from a node in the cycle.
static void CycleAdded(FbleRefArena* arena, FbleRefCallback* add, FbleRef* ref)
{
  assert(ref->cycle == NULL);

  FbleRefV stack;
  FbleVectorInit(arena->arena, stack);
  FbleVectorAppend(arena->arena, stack, ref);

  CycleAddedChildCallback callback = {
    ._base = { .callback = (void(*)(struct FbleRefCallback*, FbleRef*))&CycleAddedChild },
    .arena = arena->arena,
    .ref = ref,
    .stack = &stack,
    .add = add
  };
  InitMap(arena->arena, &callback.visited, 13);

  while (stack.size > 0) {
    arena->added(&callback._base, stack.xs[--stack.size]);
  }

  FreeMap(arena->arena, &callback.visited);
  FbleFree(arena->arena, stack.xs);
}

// FbleNewRefArena -- see documentation in ref.h
FbleRefArena* FbleNewRefArena(
    FbleArena* arena, 
    void (*free)(FbleRefArena* arena, FbleRef* ref),
    void (*added)(FbleRefCallback* add, FbleRef* ref))
{
  FbleRefArena* ref_arena = FbleAlloc(arena, FbleRefArena);
  ref_arena->arena = arena;
  ref_arena->next_id = 1;
  ref_arena->free = free;
  ref_arena->added = added;
  return ref_arena;
}

// FbleDeleteRefArena -- see documentation in ref.h
void FbleDeleteRefArena(FbleRefArena* arena)
{
  FbleFree(arena->arena, arena);
}

// FbleRefArenaArena -- see documentation in ref.h
FbleArena* FbleRefArenaArena(FbleRefArena* arena)
{
  return arena->arena;
}

// FbleRefInit -- see documentation in ref.h
void FbleRefInit(FbleRefArena* arena, FbleRef* ref)
{
  ref->id = arena->next_id++;
  ref->refcount = 1;
  ref->cycle = NULL;
}

// FbleRefRetain -- see documentation in ref.h
void FbleRefRetain(FbleRefArena* arena, FbleRef* ref)
{
  ref = CycleHead(ref);
  ref->refcount++;
}

typedef struct {
  FbleRefCallback _base;
  FbleArena* arena;
  FbleRefV* refs;
  FbleRef* ref;
  FbleRefV* in_cycle;
  FbleRefV* stack;
} RefReleaseCallback;

// RefReleaseChild --
//   Callback function used when releasing references. Decrements the
//   reference count of a ref not in the cycle, adding it to the list of nodes
//   to free if the refcount has gone to zero. Accumulates the list of nodes
//   in the cycle.
static void RefReleaseChild(RefReleaseCallback* data, FbleRef* child)
{
  if (child == data->ref || CycleHead(child) == data->ref) {
    if (child != data->ref && !Contains(data->in_cycle, child)) {
      FbleVectorAppend(data->arena, *data->in_cycle, child);
      FbleVectorAppend(data->arena, *data->stack, child);
    }
  } else {
    FbleRef* head = CycleHead(child);
    assert(head->cycle == NULL);
    assert(head->refcount > 0);
    head->refcount--;
    if (head->refcount == 0) {
      FbleVectorAppend(data->arena, *data->refs, head);
    }
  }
}

// FbleRefRelease -- see documentation in ref.h
void FbleRefRelease(FbleRefArena* arena, FbleRef* ref)
{
  ref = CycleHead(ref);
  assert(ref->cycle == NULL);
  assert(ref->refcount > 0);
  ref->refcount--;

  if (ref->refcount == 0) {
    FbleRefV refs;
    FbleVectorInit(arena->arena, refs);
    FbleVectorAppend(arena->arena, refs, ref);

    while (refs.size > 0) {
      FbleRef* r = refs.xs[--refs.size];
      assert(r->cycle == NULL);
      assert(r->refcount == 0);

      FbleRefV in_cycle;
      FbleVectorInit(arena->arena, in_cycle);
      FbleVectorAppend(arena->arena, in_cycle, r);

      FbleRefV stack;
      FbleVectorInit(arena->arena, stack);
      FbleVectorAppend(arena->arena, stack, r);

      RefReleaseCallback callback = {
        ._base = { .callback = (void(*)(struct FbleRefCallback*, FbleRef*))&RefReleaseChild },
        .arena = arena->arena,
        .refs = &refs,
        .ref = r,
        .in_cycle = &in_cycle,
        .stack = &stack,
      };

      while (stack.size > 0) {
        arena->added(&callback._base, stack.xs[--stack.size]);
      }

      for (size_t i = 0; i < in_cycle.size; ++i) {
        arena->free(arena, in_cycle.xs[i]);
      }

      FbleFree(arena->arena, in_cycle.xs);
      FbleFree(arena->arena, stack.xs);
    }

    FbleFree(arena->arena, refs.xs);
  }
}

// FbleRefAdd -- see documentation in ref.h
void FbleRefAdd(FbleRefArena* arena, FbleRef* src, FbleRef* dst)
{
  if (CycleHead(src) == CycleHead(dst)) {
    // Ignore new references within the same cycle.
    return;
  }

  FbleRefRetain(arena, dst);
  if (src->id > dst->id) {
    return;
  }

  src = CycleHead(src);
  dst = CycleHead(dst);

  // There is potentially a cycle from dst --*--> src --> dst
  // Change all nodes with ids between src->id and dst->id to src->id.
  // If any subset of those nodes form a path between dst and src, set dst as
  // their cycle and transfer their external refcount to dst.
  FbleRefV stack;
  FbleVectorInit(arena->arena, stack);

  Map visited;
  InitMap(arena->arena, &visited, 17);

  // Keep track of a reverse mapping from child to parent nodes. The child
  // node at index i is visited[i].
  struct { size_t size; FbleRefV* xs; } reverse;
  FbleVectorInit(arena->arena, reverse);

  FbleVectorAppend(arena->arena, stack, dst);
  Insert(arena->arena, &visited, dst, 0);

  {
    FbleRefV* parents = FbleVectorExtend(arena->arena, reverse);
    FbleVectorInit(arena->arena, *parents);
  }

  // Traverse all nodes reachable from dst with ids between src->id and
  // dst->id, setting their ids to src->id.
  while (stack.size > 0) {
    FbleRef* ref = stack.xs[--stack.size];
    assert(ref->cycle == NULL);
    ref->id = src->id;

    FbleRefV children;
    FbleVectorInit(arena->arena, children);
    AddToVectorCallback callback = {
      ._base = { .callback = (void(*)(struct FbleRefCallback*, FbleRef*))&AddToVector },
      .arena = arena->arena,
      .refs = &children
    };
    CycleAdded(arena, &callback._base, ref);

    for (size_t i = 0; i < children.size; ++i) {
      FbleRef* child = children.xs[i];
      if (child->id >= src->id) {
        FbleRefV* parents = NULL;
        size_t j = Insert(arena->arena, &visited, child, reverse.size);
        if (j < reverse.size) {
          parents = reverse.xs + j;
        } else {
          FbleVectorAppend(arena->arena, stack, child);
          parents = FbleVectorExtend(arena->arena, reverse);
          FbleVectorInit(arena->arena, *parents);
        }
        FbleVectorAppend(arena->arena, *parents, ref);
      }
    }
    FbleFree(arena->arena, children.xs);
  }

  // Traverse backwards from src (if reached) to identify all nodes in a newly
  // created cycle.
  FbleRefV cycle;
  FbleVectorInit(arena->arena, cycle);
  if (In(&visited, src)) {
    FbleVectorAppend(arena->arena, stack, src);
    FbleVectorAppend(arena->arena, cycle, src);
  }

  while (stack.size > 0) {
    FbleRef* ref = stack.xs[--stack.size];

    size_t i = Insert(arena->arena, &visited, ref, reverse.size);
    assert(i < reverse.size);
    FbleRefV* parents = reverse.xs + i;
    for (size_t i = 0; i < parents->size; ++i) {
      FbleRef* parent = parents->xs[i];
      if (!Contains(&cycle, parent)) {
        FbleVectorAppend(arena->arena, stack, parent);
        FbleVectorAppend(arena->arena, cycle, parent);
      }
    }
  }

  FbleFree(arena->arena, stack.xs);
  FreeMap(arena->arena, &visited);
  for (size_t i = 0; i < reverse.size; ++i) {
    FbleFree(arena->arena, reverse.xs[i].xs);
  }
  FbleFree(arena->arena, reverse.xs);

  if (cycle.size > 0) {
    // Fix up refcounts for the cycle by moving any refs from nodes outside the
    // cycle to the head node of the cycle and removing all refs from nodes
    // inside the cycle.
    size_t total = 0;
    size_t internal = 0;

    for (size_t i = 0; i < cycle.size; ++i) {
      FbleRef* ref = cycle.xs[i];
      total += ref->refcount;

      FbleRefV children;
      FbleVectorInit(arena->arena, children);
      AddToVectorCallback callback = {
        ._base = { .callback = (void(*)(struct FbleRefCallback*, FbleRef*))&AddToVector },
        .arena = arena->arena,
        .refs = &children
      };
      CycleAdded(arena, &callback._base, ref);

      // TODO: Figure out a more efficient way to check if a child is in the
      // cycle.
      for (size_t j = 0; j < children.size; ++j) {
        for (size_t k = 0; k < cycle.size; ++k) {
          if (children.xs[j] == cycle.xs[k]) {
            internal++;
          }
        }
      }

      FbleFree(arena->arena, children.xs);
    }
    
    for (size_t i = 0; i < cycle.size; ++i) {
      cycle.xs[i]->refcount = 0;
      cycle.xs[i]->cycle = dst;
    }

    dst->refcount = total - internal;
    dst->cycle = NULL;
  }

  FbleFree(arena->arena, cycle.xs);
}
