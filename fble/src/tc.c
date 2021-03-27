
#include <assert.h>     // for assert

#include "tc.h"

#define UNREACHABLE(x) assert(false && x)

// FbleFreeTc -- see documentation in tc.h
void FbleFreeTc(FbleArena* arena, FbleTc* tc)
{
  if (tc == NULL) {
    return;
  }

  switch (tc->tag) {
    case FBLE_TYPE_VALUE_TC: {
      FbleFree(arena, tc);
      return;
    }

    case FBLE_VAR_TC: {
      FbleFree(arena, tc);
      return;
    }

    case FBLE_LET_TC: {
      FbleLetTc* let_tc = (FbleLetTc*)tc;
      for (size_t i = 0; i < let_tc->bindings.size; ++i) {
        FbleFreeLoc(arena, let_tc->bindings.xs[i].loc);
        FbleFreeTc(arena, let_tc->bindings.xs[i].tc);
      }
      FbleFree(arena, let_tc->bindings.xs);
      FbleFreeTc(arena, let_tc->body);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_STRUCT_VALUE_TC: {
      FbleStructValueTc* sv = (FbleStructValueTc*)tc;
      for (size_t i = 0; i < sv->fieldc; ++i) {
        FbleFreeTc(arena, sv->fields[i]);
      }
      FbleFree(arena, tc);
      return;
    }

    case FBLE_UNION_VALUE_TC: {
      FbleUnionValueTc* uv = (FbleUnionValueTc*)tc;
      FbleFreeTc(arena, uv->arg);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_UNION_SELECT_TC: {
      FbleUnionSelectTc* v = (FbleUnionSelectTc*)tc;
      FbleFreeTc(arena, v->condition);
      FbleFreeLoc(arena, v->loc);
      for (size_t i = 0; i < v->choicec; ++i) {
        // The default branch may appear multiple times in choices. Make sure
        // we only free it once.
        bool freed = false;
        for (size_t j = 0; j < i; ++j) {
          if (v->choices[j] == v->choices[i]) {
            freed = true;
            break;
          }
        }
        if (!freed) {
          FbleFreeTc(arena, v->choices[i]);
        }
      }
      FbleFree(arena, tc);
      return;
    }

    case FBLE_DATA_ACCESS_TC: {
      FbleDataAccessTc* v = (FbleDataAccessTc*)tc;
      FbleFreeLoc(arena, v->loc);
      FbleFreeTc(arena, v->obj);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_FUNC_VALUE_TC: {
      FbleFuncValueTc* func_tc = (FbleFuncValueTc*)tc;
      FbleFreeLoc(arena, func_tc->body_loc);
      FbleFreeTc(arena, func_tc->body);
      FbleFree(arena, func_tc->scope.xs);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_FUNC_APPLY_TC: {
      FbleFuncApplyTc* apply_tc = (FbleFuncApplyTc*)tc;
      FbleFreeTc(arena, apply_tc->func);
      FbleFreeLoc(arena, apply_tc->loc);
      for (size_t i = 0; i < apply_tc->args.size; ++i) {
        FbleFreeTc(arena, apply_tc->args.xs[i]);
      }
      FbleFree(arena, apply_tc->args.xs);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_LINK_TC: {
      FbleLinkTc* link_tc = (FbleLinkTc*)tc;
      FbleFreeTc(arena, link_tc->body);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_EXEC_TC: {
      FbleExecTc* exec_tc = (FbleExecTc*)tc;
      for (size_t i = 0; i < exec_tc->bindings.size; ++i) {
        FbleFreeTc(arena, exec_tc->bindings.xs[i]);
      }
      FbleFree(arena, exec_tc->bindings.xs);
      FbleFreeTc(arena, exec_tc->body);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_PROFILE_TC: {
      FbleProfileTc* profile_tc = (FbleProfileTc*)tc;
      FbleFreeLoc(arena, profile_tc->loc);
      FbleFreeName(arena, profile_tc->name);
      FbleFreeTc(arena, profile_tc->body);
      FbleFree(arena, tc);
      return;
    }
  }

  UNREACHABLE("should never get here");
}

