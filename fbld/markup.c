
#include <assert.h>   // for assert
#include <stdbool.h>  // for false
#include <stdio.h>    // for fprintf
#include <stdlib.h>   // for abort

#include "fbld.h"
#include "vector.h"

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
FbldMarkup* FbldCopyMarkup(FbldMarkup* markup)
{
  FbldMarkup* n = malloc(sizeof(FbldMarkup));
  n->tag = markup->tag;
  n->text = NULL;
  if (markup->text) {
    n->text = malloc(sizeof(FbldText));
    n->text->loc = markup->text->loc;
    n->text->str = markup->text->str;
    n->text->str->refcount++;
  }
  FbldInitVector(n->markups);
  for (size_t i = 0; i < markup->markups.size; ++i) {
    FbldAppendToVector(n->markups, FbldCopyMarkup(markup->markups.xs[i]));
  }
  return n;
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

// See documentation in fbld.h
void FbldDebugMarkup(FbldMarkup* markup)
{
  switch (markup->tag) {
    case FBLD_MARKUP_PLAIN: {
      printf("%s", markup->text->str->str);
      break;
    }

    case FBLD_MARKUP_COMMAND: {
      printf("@{%s}", markup->text->str->str);
      for (size_t i = 0; i < markup->markups.size; ++i) {
        printf("{");
        FbldDebugMarkup(markup->markups.xs[i]);
        printf("}");
      }
      break;
    }

    case FBLD_MARKUP_SEQUENCE: {
      // TODO: Avoid this potentially deep recursion.
      for (size_t i = 0; i < markup->markups.size; ++i) {
        FbldDebugMarkup(markup->markups.xs[i]);
      }
      break;
    }
  }
}
