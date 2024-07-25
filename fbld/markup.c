
#include <assert.h>   // for assert
#include <stdbool.h>  // for false
#include <stdio.h>    // for fprintf
#include <stdlib.h>   // for abort

#include "fbld.h"

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
  if (markup->text) {
    markup->text->str->refcount--;
    if (markup->text->str->refcount == 0) {
      free(markup->text->str);
    }
    free(markup->text);
  }

  for (size_t i = 0; i < markup->markups.size; ++i) {
    FbldFreeMarkup(markup->markups.xs[i]);
  }
  free(markup->markups.xs);
  free(markup);
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
