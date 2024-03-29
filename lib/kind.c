/**
 * @file kind.c
 *  FbleKind routines.
 */

#include "kind.h"

#include <assert.h>   // for assert.
#include <stdlib.h>   // for NULL

#include <fble/fble-alloc.h>   // for FbleFree

// FbleNewBasicKind -- see documentation in kind.h
FbleKind* FbleNewBasicKind(FbleLoc loc, size_t level)
{
  FbleBasicKind* kind = FbleAlloc(FbleBasicKind);
  kind->_base.tag = FBLE_BASIC_KIND;
  kind->_base.loc = FbleCopyLoc(loc);
  kind->_base.refcount = 1;
  kind->level = level;
  return &kind->_base;
}

// FbleCopyKind -- see documentation in kind.h
FbleKind* FbleCopyKind(FbleKind* kind)
{
  assert(kind != NULL);
  kind->refcount++;
  return kind;
}

// FbleFreeKind -- see documentation in kind.h
void FbleFreeKind(FbleKind* kind)
{
  if (kind != NULL) {
    assert(kind->refcount > 0);
    kind->refcount--;
    if (kind->refcount == 0) {
      FbleFreeLoc(kind->loc);
      switch (kind->tag) {
        case FBLE_BASIC_KIND: {
          FbleFree(kind);
          break;
        }

        case FBLE_POLY_KIND: {
          FblePolyKind* poly = (FblePolyKind*)kind;
          FbleFreeKind(poly->arg);
          FbleFreeKind(poly->rkind);
          FbleFree(poly);
          break;
        }
      }
    }
  }
}
