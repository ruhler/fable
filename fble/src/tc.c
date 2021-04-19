// tc.c --
//   This file implements routines dealing with the FbleTc type.

#include "tc.h"

#include <assert.h>     // for assert

#include "fble-alloc.h"   // for FbleFree

#define UNREACHABLE(x) assert(false && x)

// FbleFreeTc -- see documentation in tc.h
void FbleFreeTc(FbleTc* tc)
{
  if (tc == NULL) {
    return;
  }

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
        FbleFreeLoc(let_tc->bindings.xs[i].var_loc);
        FbleFreeName(let_tc->bindings.xs[i].profile_name);
        FbleFreeLoc(let_tc->bindings.xs[i].profile_loc);
        FbleFreeTc(let_tc->bindings.xs[i].tc);
      }
      FbleFree(let_tc->bindings.xs);
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
      FbleFreeLoc(v->loc);
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
          FbleFreeName(v->choices.xs[i].profile_name);
          FbleFreeLoc(v->choices.xs[i].profile_loc);
          FbleFreeTc(v->choices.xs[i].tc);
        }
      }
      FbleFree(v->choices.xs);
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
      FbleFree(func_tc->scope.xs);
      FbleFree(tc);
      return;
    }

    case FBLE_FUNC_APPLY_TC: {
      FbleFuncApplyTc* apply_tc = (FbleFuncApplyTc*)tc;
      FbleFreeTc(apply_tc->func);
      FbleFreeLoc(apply_tc->loc);
      for (size_t i = 0; i < apply_tc->args.size; ++i) {
        FbleFreeTc(apply_tc->args.xs[i]);
      }
      FbleFree(apply_tc->args.xs);
      FbleFree(tc);
      return;
    }

    case FBLE_LINK_TC: {
      FbleLinkTc* link_tc = (FbleLinkTc*)tc;
      FbleFreeTc(link_tc->body);
      FbleFree(tc);
      return;
    }

    case FBLE_EXEC_TC: {
      FbleExecTc* exec_tc = (FbleExecTc*)tc;
      for (size_t i = 0; i < exec_tc->bindings.size; ++i) {
        FbleFreeName(exec_tc->bindings.xs[i].profile_name);
        FbleFreeLoc(exec_tc->bindings.xs[i].profile_loc);
        FbleFreeTc(exec_tc->bindings.xs[i].tc);
      }
      FbleFree(exec_tc->bindings.xs);
      FbleFreeTc(exec_tc->body);
      FbleFree(tc);
      return;
    }
  }

  UNREACHABLE("should never get here");
}

