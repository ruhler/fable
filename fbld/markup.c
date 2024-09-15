
#include <assert.h>   // for assert
#include <stdbool.h>  // for false
#include <stdio.h>    // for fprintf
#include <stdlib.h>   // for abort
#include <string.h>   // for strlen

#include "alloc.h"
#include "fbld.h"
#include "vector.h"

static bool TextOfMarkup(FbldMarkup* markup, FbldText** text, size_t* capacity);

// See documentation in fbld.h
void FbldReportError(FbldLoc loc, const char* message)
{
  fprintf(stderr, "%s:%zi:%zi: error: %s\n",
      loc.file, loc.line, loc.column, message);
}

// See documentation in fbld.h
FbldText* FbldNewText(FbldLoc loc, const char* str)
{
  FbldText* text = FbldAllocExtra(FbldText, strlen(str) + 1);
  text->loc = loc;
  strcpy(text->str, str);
  return text;
}

// See documentation in fbld.h
void FbldFreeMarkup(FbldMarkup* markup)
{
  if (markup == NULL) {
    return;
  }

  markup->refcount--;
  if (markup->refcount > 0) {
    return;
  }

  if (markup->text) {
    FbldFree(markup->text);
  }

  for (size_t i = 0; i < markup->markups.size; ++i) {
    FbldFreeMarkup(markup->markups.xs[i]);
  }
  FbldFree(markup->markups.xs);
  FbldFree(markup);
}

// See documentation in fbld.h
FbldMarkup* FbldCopyMarkup(FbldMarkup* markup)
{
  markup->refcount++;
  return markup;
}

// See documentation in fbld.h
FbldLoc FbldMarkupLoc(FbldMarkup* markup)
{
  switch (markup->tag) {
    case FBLD_MARKUP_PLAIN: return markup->text->loc;
    case FBLD_MARKUP_COMMAND: return markup->text->loc;
    case FBLD_MARKUP_SEQUENCE: {
      if (markup->markups.size == 0) {
        // TODO: Track locations properly in this case.
        FbldLoc loc = { .file = "???", .line = 1, .column = 1 };
        return loc;
      }
      return FbldMarkupLoc(markup->markups.xs[0]);
    }
  }

  assert(false && "Unreachable");
  return markup->text->loc;
}

/**
 * @func[TextOfMarkup] Helper function for FbldTextOfMarkup
 *  @arg[markup] Markup to convert.
 *  @arg[FbldText**] Text constructed so far. Value may be NULL.
 *  @arg[size_t*][available]
 *   Available unused size allocated for text str.
 *  @returns[bool] True on success, false in case of error.
 *  @sideeffects
 *   @i Reallocates FbldText* if needed and populates it with text.
 *   @i Prints message to stderr in case of error.
 */
static bool TextOfMarkup(FbldMarkup* markup, FbldText** text, size_t* capacity)
{
  switch (markup->tag) {
    case FBLD_MARKUP_PLAIN: {
      if (*text == NULL) {
        *text = FbldNewText(markup->text->loc, markup->text->str);
        *capacity = 0;
        return true;
      }

      size_t src_size = strlen(markup->text->str);
      if (src_size >= *capacity) {
        size_t size = strlen((*text)->str) + *capacity;
        size_t nsize = 2 * (strlen((*text)->str) + src_size);
        *text = FbldReAllocRaw(*text, sizeof(FbldText) + nsize);
        *capacity = nsize - size;
        assert(src_size < *capacity);
      }

      *capacity -= src_size;
      strcat((*text)->str, markup->text->str);
      return true;
    }

    case FBLD_MARKUP_COMMAND: {
      FbldReportError(markup->text->loc, "expected plain text, but found command");
      return false;
    }

    case FBLD_MARKUP_SEQUENCE: {
      // TODO: Avoid this potentially deep recursion.
      for (size_t i = 0; i < markup->markups.size; ++i) {
        if (!TextOfMarkup(markup->markups.xs[i], text, capacity)) {
          if (*text != NULL) {
            FbldFree(*text);
            *text = NULL;
          }
          return false;
        }
      }
      return true;
    }
  }

  assert(false && "unreachable");
  return false;
}

// See documentation in fbld.h
FbldText* FbldTextOfMarkup(FbldMarkup* markup)
{
  FbldText* text = NULL;
  size_t capacity = 0;
  if (!TextOfMarkup(markup, &text, &capacity)) {
    return NULL;
  }
  if (text == NULL) {
    return FbldNewText(FbldMarkupLoc(markup), "");
  }
  return text;
}

// See documentation in fbld.h
bool FbldPrintMarkup(FbldMarkup* markup)
{
  switch (markup->tag) {
    case FBLD_MARKUP_PLAIN: {
      printf("%s", markup->text->str);
      return true;
    }

    case FBLD_MARKUP_COMMAND: {
      FbldReportError(markup->text->loc, "expected plain text, but found command");
      return false;
    }

    case FBLD_MARKUP_SEQUENCE: {
      // TODO: Avoid this potentially deep recursion.
      for (size_t i = 0; i < markup->markups.size; ++i) {
        if (!FbldPrintMarkup(markup->markups.xs[i])) {
          return false;
        }
      }
      return true;
    }
  }

  assert(false && "unreachable");
  return false;
}

// See documentation in fbld.h
void FbldDebugMarkup(FbldMarkup* markup)
{
  switch (markup->tag) {
    case FBLD_MARKUP_PLAIN: {
      printf("%s", markup->text->str);
      break;
    }

    case FBLD_MARKUP_COMMAND: {
      printf("@{%s}", markup->text->str);
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
