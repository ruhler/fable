
#ifndef FBLC_GC_H_
#define FBLC_GC_H_

#include "fblc.h"

// GcInit --
//   Initialze the GC infrastucture.
//
// Inputs:
//   None.
//
// Results:
//   None.
//
// Side effects:
//   Initializes the GC infrastructure. After this, GCArenas may be used.
//   The GC infrastructure should be torn down with GcFinish() when done.
void GcInit();

// GcFinish --
//   Tear down the GC infrastucture.
//
// Inputs:
//   None.
//
// Results:
//   None.
//
// Side effects:
//   Tears down the GC infrastructure.
void GcFinish();

// CreateGcArena --
//   Create an arena that using the GC infrastructure for allocations.
//
// Inputs:
//   None.
//
// Results:
//   A new GcArena.
//
// Side effects:
//   A new GcArena is allocated using the GC infrastructure. The GcArena must
//   be freed using FreeGcArena when the arena is no longer needed.
FblcArena* CreateGcArena();

// FreeGcArena --
//   Free a GcArena created with CreateGcArena.
//
// Inputs:
//   arena - The arena to free.
//
// Results:
//   None.
//
// Side effects:
//   Frees resources associated with the given arena. The behavior is
//   undefined if there are oustanding allocations within the arena that have
//   not been explicitly freed. The behavior is undefined if the arena was
//   not created using CreateGcArena.
void FreeGcArena(FblcArena* arena);

// CreateBulkFreeArena --
//   Create a bulk-free arena on top of the given arena.
//   A bulk-free arena keeps track of all alocations performed and frees them
//   all at once when FreeBulkFreeArena is called.
//
// Inputs:
//   arena - The underlying arena to use for this arena's allocations.
//
// Results:
//   A new bulk-free arena.
//
// Side effects:
//   A new arena is allocated using the given arena. The new arena must be be
//   freed using FreeBulkFreeArena when the arena is no longer needed.
FblcArena* CreateBulkFreeArena(FblcArena* arena);

// FreeBulkFreeArena --
//   Free a bulk-free arena along with any allocations made by the arena.
//
// Inputs:
//   arena - The bulk-free arena free.
//
// Results:
//   None.
//
// Side effects:
//   All allocations through the bulk-arena are freed, along with any other
//   resources associated with the arena and the arena itself. The behavior
//   is undefined if the arena was not created using CreateBulkFreeArena.
void FreeBulkFreeArena(FblcArena* arena);

#endif//FBLC_GC_H_
