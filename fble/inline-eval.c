// inline-eval.c
//   This file implements inline evaluation.

#include <assert.h>   // for assert

#include "fble-value.h"
#include "inline-eval.h"
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

// FbleInlineEval -- see documentation in inline-eval.h
FbleValue* FbleInlineEval(FbleValueHeap* heap, FbleValue* expr)
{
  switch (expr->tag) {
    case FBLE_STRUCT_VALUE: {
      FbleStructValue* struct_value = (FbleStructValue*)expr;
      FbleStructValue* result = FbleNewValueExtra(heap, FbleStructValue, sizeof(FbleValue*) * struct_value->fieldc);
      result->_base.tag = FBLE_STRUCT_VALUE;
      result->fieldc = struct_value->fieldc;
      bool error = false;
      for (size_t i = 0; i < struct_value->fieldc; ++i) {
        // TODO: Don't rely on recursion for this, because we could smash the
        // stack.
        result->fields[i] = FbleInlineEval(heap, struct_value->fields[i]);
        if (result->fields[i] == NULL) {
          error = true;
        } else {
          FbleValueAddRef(heap, &result->_base, result->fields[i]);
          FbleReleaseValue(heap, result->fields[i]);
        }
      }

      if (error) {
        FbleReleaseValue(heap, &result->_base);
        return NULL;
      }
      return &result->_base;
    }

    case FBLE_UNION_VALUE: {
      FbleUnionValue* union_value = (FbleUnionValue*)expr;
      FbleUnionValue* result = FbleNewValue(heap, FbleUnionValue);
      result->_base.tag = FBLE_UNION_VALUE;
      result->tag = union_value->tag;
      result->arg = FbleInlineEval(heap, union_value->arg);
      if (result->arg == NULL) {
        FbleReleaseValue(heap, &result->_base);
        return NULL;
      }

      FbleValueAddRef(heap, &result->_base, result->arg);
      FbleReleaseValue(heap, result->arg);
      return &result->_base;
    }

    case FBLE_STRUCT_ACCESS_VALUE: {
      assert(false && "TODO: inline eval struct access");
      return NULL;
    }

    case FBLE_UNION_ACCESS_VALUE: {
      assert(false && "TODO: inline eval union access");
      return NULL;
    }

    case FBLE_FUNC_VALUE:
    case FBLE_LINK_VALUE:
    case FBLE_PORT_VALUE:
    case FBLE_REF_VALUE:
    case FBLE_TYPE_VALUE: {
      assert(false && "poorly typed arg to inline eval");
      return NULL;
    }
  }

  UNREACHABLE("should never get here");
 return NULL; 
}

