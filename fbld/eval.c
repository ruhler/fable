
#include <assert.h>   // for assert
#include <stdbool.h>  // for false
#include <stdlib.h>   // for malloc
#include <string.h>   // for strcmp

#include "fbld.h"


FbldMarkup* FbldEval(FbldMarkup* markup)
{
  switch (markup->tag) {
    case FBLD_MARKUP_PLAIN: {
      // TODO: Use reference counting to avoid a copy here?
      FbldMarkup* m = malloc(sizeof(FbldMarkup));
      m->tag = FBLD_MARKUP_PLAIN;
      m->text = malloc(sizeof(FbldText));
      m->text->loc = markup->text->loc;
      m->text->str = markup->text->str;
      m->text->str->refcount++;
      FbldInitVector(&m->markups);
      return m;
    }

    case FBLD_MARKUP_COMMAND: {
      if (strcmp(markup->text->str->str, "error") == 0) {
        if (markup->markups.size != 1) {
          FbldError(markup->text->loc, "expected 1 argument to @error");
          return NULL;
        }

        FbldMarkup* arg = markup->markups.xs[0];
        if (arg->tag != FBLD_MARKUP_PLAIN) {
          // TODO: Sequence of plain ought to be allowed too, right?
          FbldError(markup->text->loc, "argument to @error is not plain");
          return NULL;
        }

        FbldError(markup->text->loc, arg->text->str->str);
        return NULL;
      }

      if (strcmp(markup->text->str->str, "define") == 0) {
        assert(false && "TODO: implement @define");
        return NULL;
      }

      if (strcmp(markup->text->str->str, "ifeq") == 0) {
        assert(false && "TODO: implement @ifeq");
        return NULL;
      }

      FbldError(markup->text->loc, "unsupported command");
      return NULL;
    }

    case FBLD_MARKUP_SEQUENCE: {
      FbldMarkup* m = malloc(sizeof(FbldMarkup));
      m->tag = FBLD_MARKUP_SEQUENCE;
      m->text = NULL;
      FbldInitVector(&m->markups);
      for (size_t i = 0; i < markup->markups.size; ++i) {
        FbldAppendToVector(&m->markups, FbldEval(markup->markups.xs[i]));
      }
      return m;
    }
  }

  return NULL;
}
