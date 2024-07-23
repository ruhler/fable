
#include <assert.h>   // for assert
#include <stdbool.h>  // for false
#include <stdio.h>    // for fprintf
#include <stdlib.h>   // for abort

#include "fbld.h"

// See documenetation in fbld.h
void FbldInitVector(FbldMarkupV* v)
{
  // Initial size is 0. Initial capacity is 1.
  v->size = 0;
  v->xs = malloc(sizeof(FbldMarkup*));
}

// See documenetation in fbld.h
void FbldAppendToVector(FbldMarkupV* v, FbldMarkup* e)
{
  // We assume the capacity of the array is the smallest power of 2 that holds
  // size elements. If size is equal to the capacity of the array, we double
  // the capacity of the array, which preserves the invariant after the size is
  // incremented.
  size_t s = v->size++;
  if (s > 0 && (s & (s - 1)) == 0) {
    v->xs = realloc(v->xs, 2 * s * sizeof(FbldMarkup*));
  }
  v->xs[s] = e;
}

// See documenetation in fbld.h
void FbldFreeVector(FbldMarkupV v)
{
  free(v.xs);
}

// See documentation in fbld.h
void FbldError(FbldLoc loc, const char* message)
{
  fprintf(stderr, "%s:%zi:%zi: error: %s\n",
      loc.file, loc.line, loc.column, message);
  abort();
}

// See documentation in fbld.h
void FbldFreeMarkup(FbldMarkup* markup)
{
  assert(false && "TODO: implement FbldFreeMarkup");
}

// See documentation in fbld.h
void FbldPrintMarkup(FbldMarkup* markup)
{
  switch (markup->tag) {
    case FBLD_MARKUP_PLAIN: {
      printf("%s", markup->text->str->str);
      break;
    }

    case FBLD_MARKUP_COMMAND: {
      FbldError(markup->text->loc, "expected plain text, but found command");
      break;
    }

    case FBLD_MARKUP_SEQUENCE: {
      // TODO: Avoid this potentially deep recursion.
      for (size_t i = 0; i < markup->markups.size; ++i) {
        FbldPrintMarkup(markup->markups.xs[i]);
      }
      break;
    }
  }
}
