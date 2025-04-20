/**
 * @file tc.c
 *  FbleTc abstract syntax routines.
 */

#include "tc.h"

#include <assert.h>     // for assert

#include <fble/fble-alloc.h>   // for FbleFree
#include <fble/fble-vector.h>  // for FbleFreeVector

#include "unreachable.h"

static void FreeTcBinding(FbleTcBinding binding);


/**
 * @func[FreeTcBinding] Frees resources associated with an FbleTcBinding]
 *  @arg[FbleTcBinding][binding] The binding to free resources for.
 *
 *  @sideeffects
 *   Frees resources associated with @a[binding].
 */
static void FreeTcBinding(FbleTcBinding binding)
{
  FbleFreeName(binding.name);
  FbleFreeLoc(binding.loc);
  FbleFreeTc(binding.tc);
}

// FbleNewTcRaw -- see documentation in tc.h
FbleTc* FbleNewTcRaw(size_t size, FbleTcTag tag, FbleLoc loc)
{
  FbleTc* tc = (FbleTc*)FbleAllocRaw(size);
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
        FreeTcBinding(let_tc->bindings.xs[i]);
      }
      FbleFreeVector(let_tc->bindings);
      FbleFreeTc(let_tc->body);
      FbleFree(tc);
      return;
    }

    case FBLE_UNDEF_TC: {
      FbleUndefTc* undef_tc = (FbleUndefTc*)tc;
      FbleFreeName(undef_tc->name);
      FbleFreeTc(undef_tc->body);
      FbleFree(tc);
      return;
    }

    case FBLE_STRUCT_VALUE_TC: {
      FbleStructValueTc* sv = (FbleStructValueTc*)tc;
      for (size_t i = 0; i < sv->fields.size; ++i) {
        FbleFreeTc(sv->fields.xs[i]);
      }
      FbleFreeVector(sv->fields);
      FbleFree(tc);
      return;
    }

    case FBLE_STRUCT_ACCESS_TC: {
      FbleStructAccessTc* v = (FbleStructAccessTc*)tc;
      FbleFreeLoc(v->loc);
      FbleFreeTc(v->obj);
      FbleFree(tc);
      return;
    }

    case FBLE_STRUCT_COPY_TC: {
      FbleStructCopyTc* s = (FbleStructCopyTc*)tc;
      FbleFreeTc(s->source);
      for (size_t i = 0; i < s->fields.size; ++i) {
        FbleFreeTc(s->fields.xs[i]);
      }
      FbleFreeVector(s->fields);
      FbleFree(tc);
      return;
    }

    case FBLE_UNION_VALUE_TC: {
      FbleUnionValueTc* uv = (FbleUnionValueTc*)tc;
      FbleFreeTc(uv->arg);
      FbleFree(tc);
      return;
    }

    case FBLE_UNION_ACCESS_TC: {
      FbleUnionAccessTc* v = (FbleUnionAccessTc*)tc;
      FbleFreeLoc(v->loc);
      FbleFreeTc(v->obj);
      FbleFree(tc);
      return;
    }

    case FBLE_UNION_SELECT_TC: {
      FbleUnionSelectTc* v = (FbleUnionSelectTc*)tc;
      FbleFreeTc(v->condition);
      for (size_t i = 0; i < v->targets.size; ++i) {
        FreeTcBinding(v->targets.xs[i].target);
      }
      FbleFreeVector(v->targets);
      FreeTcBinding(v->default_);
      FbleFree(tc);
      return;
    }

    case FBLE_FUNC_VALUE_TC: {
      FbleFuncValueTc* func_tc = (FbleFuncValueTc*)tc;
      FbleFreeLoc(func_tc->body_loc);
      FbleFreeTc(func_tc->body);
      FbleFreeVector(func_tc->scope);

      for (size_t i = 0; i < func_tc->statics.size; ++i) {
        FbleFreeName(func_tc->statics.xs[i]);
      }
      FbleFreeVector(func_tc->statics);

      for (size_t i = 0; i < func_tc->args.size; ++i) {
        FbleFreeName(func_tc->args.xs[i]);
      }
      FbleFreeVector(func_tc->args);
      FbleFree(tc);
      return;
    }

    case FBLE_FUNC_APPLY_TC: {
      FbleFuncApplyTc* apply_tc = (FbleFuncApplyTc*)tc;
      FbleFreeTc(apply_tc->func);
      FbleFreeTc(apply_tc->arg);
      FbleFree(tc);
      return;
    }

    case FBLE_LIST_TC: {
      FbleListTc* v = (FbleListTc*)tc;
      for (size_t i = 0; i < v->fields.size; ++i) {
        FbleFreeTc(v->fields.xs[i]);
      }
      FbleFreeVector(v->fields);
      FbleFree(tc);
      return;
    }

    case FBLE_LITERAL_TC: {
      FbleLiteralTc* v = (FbleLiteralTc*)tc;
      FbleFreeVector(v->letters);
      FbleFree(tc);
      return;
    }
  }

  FbleUnreachable("should never get here");
}
