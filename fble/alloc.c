// alloc.c --
//   This file implements the fble allocation routines.

#include "fble.h"

// Alloc --
//   An allocation managed by an fble arena. Forms a circular doubly linked
//   list of allocations belonging to the same arena.
typedef struct Alloc {
  Alloc* prev;
  Alloc* next;
  const char* msg;
  char data[];
} Alloc;

// Arena --
//   A circularly doubly linked list of child arenas.
typedef struct Arena {
  Arena* prev;
  Arena* next;
  FbleArena arena;
} Arena;

// FbleArena -- See fble.h for documentation.
//
// Fields:
//   allocs - The list of allocations for this arena. It is a circular, doubly
//            linked list with the first node a dummy node.
//   arenas - The list of child arenas for this arena. It is a circular,
//            doubly linked list with the first node a dummy node.
struct FbleArena {
  Alloc* allocs;
  Arena* arenas;
};

