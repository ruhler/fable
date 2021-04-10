
#include <assert.h>     // for assert

#include "tc.h"

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
        FbleFreeLoc(let_tc->bindings.xs[i].loc);
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
          FbleFreeTc(v->choices[i]);
        }
      }
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
        FbleFreeTc(exec_tc->bindings.xs[i]);
      }
      FbleFree(exec_tc->bindings.xs);
      FbleFreeTc(exec_tc->body);
      FbleFree(tc);
      return;
    }

    case FBLE_PROFILE_TC: {
      FbleProfileTc* profile_tc = (FbleProfileTc*)tc;
      FbleFreeLoc(profile_tc->loc);
      FbleFreeName(profile_tc->name);
      FbleFreeTc(profile_tc->body);
      FbleFree(tc);
      return;
    }
  }

  UNREACHABLE("should never get here");
}

