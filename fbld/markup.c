
#include <assert.h>   // for assert
#include <stdbool.h>  // for false
#include <stdio.h>    // for fprintf
#include <stdlib.h>   // for abort
#include <string.h>   // for strlen

#include "fbld.h"
#include "vector.h"

static void TextOfMarkup(FbldMarkup* markup, FbldText** text, size_t* capacity);

// See documentation in fbld.h
void FbldError(FbldLoc loc, const char* message)
{
  fprintf(stderr, "%s:%zi:%zi: error: %s\n",
      loc.file, loc.line, loc.column, message);
  abort();
}

// See documentation in fbld.h
FbldText* FbldNewText(FbldLoc loc, const char* str)
{
  FbldText* text = malloc(sizeof(FbldText) + sizeof(char) * strlen(str) + 1);
  text->loc = loc;
  strcpy(text->str, str);
  return text;
}

// See documentation in fbld.h
void FbldFreeMarkup(FbldMarkup* markup)
{
  markup->refcount--;
  if (markup->refcount > 0) {
    return;
  }

  if (markup->text) {
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
 *  @sideeffects
 *   @i Reallocates FbldText* if needed and populates it with text.
 *   @i Errors out if the markup has unevaluated commands.
 */
static void TextOfMarkup(FbldMarkup* markup, FbldText** text, size_t* capacity)
{
  switch (markup->tag) {
    case FBLD_MARKUP_PLAIN: {
      if (*text == NULL) {
        *text = FbldNewText(markup->text->loc, markup->text->str);
        *capacity = 0;
        return;
      }

      size_t src_size = strlen(markup->text->str);
      if (src_size >= *capacity) {
        size_t size = strlen((*text)->str) + *capacity;
        size_t nsize = 2 * (strlen((*text)->str) + src_size);
        *text = realloc(*text, sizeof(FbldText) + nsize);
        *capacity = nsize - size;
        assert(src_size < *capacity);
      }

      *capacity -= src_size;
      strcat((*text)->str, markup->text->str);
      return;
    }

    case FBLD_MARKUP_COMMAND: {
      FbldError(markup->text->loc, "expected plain text, but found command");
      return;
    }

    case FBLD_MARKUP_SEQUENCE: {
      // TODO: Avoid this potentially deep recursion.
      for (size_t i = 0; i < markup->markups.size; ++i) {
        TextOfMarkup(markup->markups.xs[i], text, capacity);
      }
      return;
    }
  }
}

// See documentation in fbld.h
FbldText* FbldTextOfMarkup(FbldMarkup* markup)
{
  FbldText* text = NULL;
  size_t capacity = 0;
  TextOfMarkup(markup, &text, &capacity);
  if (text == NULL) {
    return FbldNewText(FbldMarkupLoc(markup), "");
  }
  return text;
}

// See documentation in fbld.h
void FbldPrintMarkup(FbldMarkup* markup)
{
  switch (markup->tag) {
    case FBLD_MARKUP_PLAIN: {
      printf("%s", markup->text->str);
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
