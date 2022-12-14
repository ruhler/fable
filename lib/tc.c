/**
 * @file tc.c
 * FbleTc abstract syntax routines.
 */

#include "tc.h"

#include <assert.h>     // for assert

#include <fble/fble-alloc.h>   // for FbleFree
#include <fble/fble-vector.h>  // for FbleVectorFree

#include "unreachable.h"

// FbleNewTcRaw -- see documentation in tc.h
FbleTc* FbleNewTcRaw(size_t size, FbleTcTag tag, FbleLoc loc)
{
  FbleTc* tc = (FbleTc*)FbleRawAlloc(size);
  tc->refcount = 1;
  tc->magic = FBLE_TC_MAGIC;
  tc->tag = tag;
  tc->loc = FbleCopyLoc(loc);
  return tc;
}

// FbleCopyTc -- see documentation in tc.h
FbleTc* FbleCopyTc(FbleTc* tc)
{
  tc->refcount++;
  return tc;
}

// FbleFreeTc -- see documentation in tc.h
void FbleFreeTc(FbleTc* tc)
{
  if (tc == NULL) {
    return;
  }

  // If the tc magic is wrong, the tc is corrupted. That suggests we have
  // already freed this tc, and that something is messed up with tracking
  // FbleTc refcounts. 
  assert(tc->magic == FBLE_TC_MAGIC && "corrupt FbleTc");
  if (--tc->refcount > 0) {
    return;
  }

  FbleFreeLoc(tc->loc);

  switch (tc->tag) {
    case FBLE_TYPE_VALUE_TC: {
      FbleFree(tc);
      return;
    }

    case FBLE_VAR_TC: {
      FbleFree(tc);
      return;
    }

    case FBLE_LET_TC: {
      FbleLetTc* let_tc = (FbleLetTc*)tc;
      for (size_t i = 0; i < let_tc->bindings.size; ++i) {
        FbleFreeName(let_tc->bindings.xs[i].name);
        FbleFreeLoc(let_tc->bindings.xs[i].loc);
        FbleFreeTc(let_tc->bindings.xs[i].tc);
      }
      FbleVectorFree(let_tc->bindings);
      FbleFreeTc(let_tc->body);
      FbleFree(tc);
      return;
    }

    case FBLE_STRUCT_VALUE_TC: {
      FbleStructValueTc* sv = (FbleStructValueTc*)tc;
      for (size_t i = 0; i < sv->fieldc; ++i) {
        FbleFreeTc(sv->fields[i]);
      }
      FbleFree(tc);
      return;
    }

    case FBLE_UNION_VALUE_TC: {
      FbleUnionValueTc* uv = (FbleUnionValueTc*)tc;
      FbleFreeTc(uv->arg);
      FbleFree(tc);
      return;
    }

    case FBLE_UNION_SELECT_TC: {
      FbleUnionSelectTc* v = (FbleUnionSelectTc*)tc;
      FbleFreeTc(v->condition);
      for (size_t i = 0; i < v->choices.size; ++i) {
        // The default branch may appear multiple times in choices. Make sure
        // we only free it once.
        bool freed = false;
        for (size_t j = 0; j < i; ++j) {
          if (v->choices.xs[j].tc == v->choices.xs[i].tc) {
            freed = true;
            break;
          }
        }
        if (!freed) {
          FbleFreeName(v->choices.xs[i].name);
          FbleFreeLoc(v->choices.xs[i].loc);
          FbleFreeTc(v->choices.xs[i].tc);
        }
      }
      FbleVectorFree(v->choices);
      FbleFree(tc);
      return;
    }

    case FBLE_DATA_ACCESS_TC: {
      FbleDataAccessTc* v = (FbleDataAccessTc*)tc;
      FbleFreeLoc(v->loc);
      FbleFreeTc(v->obj);
      FbleFree(tc);
      return;
    }

    case FBLE_FUNC_VALUE_TC: {
      FbleFuncValueTc* func_tc = (FbleFuncValueTc*)tc;
      FbleFreeLoc(func_tc->body_loc);
      FbleFreeTc(func_tc->body);
      FbleVectorFree(func_tc->scope);

      for (size_t i = 0; i < func_tc->statics.size; ++i) {
        FbleFreeName(func_tc->statics.xs[i]);
      }
      FbleVectorFree(func_tc->statics);

      for (size_t i = 0; i < func_tc->args.size; ++i) {
        FbleFreeName(func_tc->args.xs[i]);
      }
      FbleVectorFree(func_tc->args);
      FbleFree(tc);
      return;
    }

    case FBLE_FUNC_APPLY_TC: {
      FbleFuncApplyTc* apply_tc = (FbleFuncApplyTc*)tc;
      FbleFreeTc(apply_tc->func);
      for (size_t i = 0; i < apply_tc->args.size; ++i) {
        FbleFreeTc(apply_tc->args.xs[i]);
      }
      FbleVectorFree(apply_tc->args);
      FbleFree(tc);
      return;
    }

    case FBLE_LIST_TC: {
      FbleListTc* v = (FbleListTc*)tc;
      for (size_t i = 0; i < v->fieldc; ++i) {
        FbleFreeTc(v->fields[i]);
      }
      FbleFree(tc);
      return;
    }

    case FBLE_LITERAL_TC: {
      FbleFree(tc);
      return;
    }
  }

  FbleUnreachable("should never get here");
}

