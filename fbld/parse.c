
#include <assert.h>   // for assert
#include <ctype.h>    // for isalnum
#include <stdbool.h>  // for false
#include <stdio.h>    // for FILE, feof, etc.
#include <stdlib.h>   // for abort
#include <string.h>   // for strcpy

#include "alloc.h"
#include "fbld.h"
#include "vector.h"

/**
 * @value[END] The character used to denote end of input.
 *  @type[char]
 */
const char END = '\x03';

/**
 * @struct[Lex] State of the lexer.
 *  @field[const char**][inputs] Remaining input files to process.
 *  @field[FILE*][fin] The current input file being processed.
 *  @field[FbldLoc][loc] The current location.
 *  @field[size_t][indent] The current block indent level.
 *  @field[char*][next] A buffer for storing the next characters in the input.
 *  @field[size_t][next_size] The number of characters in next.
 *  @field[size_t][next_capacity] The allocated size of next.
 */
typedef struct {
  const char** inputs;
  FILE* fin;
  FbldLoc loc;

  size_t indent;

  char* next;
  size_t next_size;
  size_t next_capacity;
} Lex;

// The context we are parsing inline text from.
//  INLINE_ARG - Inside a [...] block.
//  IMPLICIT_BLOCK - Parsing implicit block text.
//  SAME_LINE_ARG - From a same line arg.
typedef enum {
  INLINE_ARG,
  IMPLICIT_BLOCK,
  SAME_LINE_ARG
} InlineContext;

static char GetC(Lex* lex);
static char NextFetched(Lex* lex, size_t* i);
static char Char(Lex* lex);
static bool Is(Lex* lex, const char* str);
static bool IsEnd(Lex* lex);
static void Advance(Lex* lex);

static bool IsNameChar(int c);
static FbldText* ParseName(Lex* lex);
static bool ParseInlineArgs(Lex* lex, FbldMarkupV* args);
static FbldMarkup* ParseInlineCommand(Lex* lex);
static FbldMarkup* ParseInline(Lex* lex, InlineContext context);
static FbldMarkup* ParseBlockCommand(Lex* lex);
static FbldMarkup* ParseBlock(Lex* lex);

/**
 * @func[GetC] Fetches another input character.
 *  Does not cross input files unless current next buffer is empty. Treats
 *  unindented lines as end of input.
 *
 *  @arg[Lex*][lex] The lexer state.
 *  @returns[char] The input character fetched, or END in case of end of input.
 *  @sideeffects Advances location to next file if appropriate.
 */
static char GetC(Lex* lex)
{
  int c = EOF;
  if (lex->fin != NULL) {
    c = fgetc(lex->fin);
  }

  while (lex->next_size == 0 && c == EOF) {
    const char* filename = *lex->inputs;
    if (filename == NULL) {
      // We've finished processing all the inputs.
      return END;
    }
    lex->inputs++;

    if (lex->fin) {
      fclose(lex->fin);
    }

    // Open the next input file for processing.
    if (strcmp(filename, "-") == 0) {
      lex->fin = stdin;
    } else {
      lex->fin = fopen(filename, "r");
      if (lex->fin == NULL) {
        fprintf(stderr, "ERROR: unable to open '%s' for reading\n", filename); 
        abort();
      }
    }
    lex->loc.file = filename;
    lex->loc.line = 1;
    lex->loc.column = 1;

    c = fgetc(lex->fin);
  }
  return c == EOF ? END : c;
}

/**
 * @func[NextFetched] Gets the next fetched character.
 *  Takes into account current indent level and position and fetches
 *  characters into 'next' as needed.
 *
 *  @arg[Lex*][lex] The lexer state.
 *  @arg[size_t*][i] Index into lex->next to start fetching from.
 *  @returns The next character in the input, given current indent level.
 *  @sideeffects Updates col, and i to point to the next character.
 */
static char NextFetched(Lex* lex, size_t* i)
{
  // Assume we traverse a single index at at a time starting from 0. Callers
  // should not be giving random indexes for i.
  assert(*i <= lex->next_size);

  // Compute the current column.
  // TODO: Should we make this more efficient?
  size_t col = lex->loc.column;
  for (size_t j = 0; j < *i; ++j) {
    if (lex->next[j] == '\n') {
      // Note: I think this is unreachable because we have no calls to the Is
      // function with newline not at the end of the string.
      col = 1;
    } else {
      col++;
    }
  }

  while (true) {
    // Fetch another character into the 'next' buffer if needed.
    if (lex->next_size == *i) {
      char c = GetC(lex);
      if (c == END) {
        return END;
      }

      if (lex->next_size == lex->next_capacity) {
        lex->next_capacity *= 2;
        lex->next = FbldReAllocArray(char, lex->next, lex->next_capacity);
      }

      lex->next[lex->next_size] = c;
      lex->next_size++;
    }

    if (col < lex->indent + 1) {
      if (lex->next[*i] == ' ') {
        col++;
        (*i)++;
        continue;
      }

      if (lex->next[*i] == '\n') {
        return '\n';
      }

      // Unindented text is treated as 'END'.
      return END;
    }

    return lex->next[*i];
  }
}

/**
 * @func[Char] Returns the next input character.
 *  @arg[Lex*][lex] The lexer state.
 *  @returns[char] The next input character, or END in case of end of input.
 *  @sideeffects Fetches the next input character if needed.
 */
static char Char(Lex* lex)
{
  size_t i = 0;
  return NextFetched(lex, &i);
}

/**
 * @func[Is] Test if the next characters in the input match str.
 *  The match is not allowed to span different input files.
 *
 *  @arg[Lex*][lex] The lexer state.
 *  @arg[const char*][str] The string to match.
 *  @returns[bool] true if the next characters match str, false otherwise.
 *  @sideeffects Fetches the next input characters as needed.
 */
static bool Is(Lex* lex, const char* str)
{
  for (size_t i = 0; *str != '\0'; ++i) {
    char c = NextFetched(lex, &i);
    if (c == END) {
      return false;
    }

    // Check if next character matches the input string.
    if (*str != c) {
      return false;
    }

    str++;
  }
  return true;
}

/**
 * @func[IsEnd] Checks if we've reached the end of input.
 *  Taking into account the current indent level.
 *
 *  @arg[Lex*][lex] The lexer state.
 *  @returns[bool] True if we are at the end of input, false otherwise.
 *  @sideeffects Fetches the next input character if needed.
 */
static bool IsEnd(Lex* lex)
{
  return END == Char(lex);
}

/**
 * @func[Advance] Advances to the next character in the input.
 *  @arg[Lex*][lex] The lexer state.
 *  @sideeffects Advances to the next character in the input.
 */
static void Advance(Lex* lex)
{
  size_t index = 0;
  char c = NextFetched(lex, &index);
  assert(c != END);
  size_t len = index+1;

  for (size_t i = 0; i < len; ++i) {
    // Advance the location.
    switch (lex->next[i]) {
      case '\n':
        lex->loc.line++;
        lex->loc.column = 1;
        break;

      default:
        lex->loc.column++;
    }
  }
  lex->next_size -= len;
  memmove(lex->next, lex->next + len, lex->next_size);
}

/**
 * @func[IsNameChar] Tests if a character is a name character
 *  @arg[int][c] The character to test.
 *  @returns[bool] True if the char is a name character.
 *  @sideeffects None
 */
static bool IsNameChar(int c)
{
  return c == '_' || isalnum(c);
}

/**
 * @func[ParseName] Parses an fbld command name.
 *  @arg[Lex*][lex] The lexer state.
 *  @returns[FbldText*] The parsed command name.
 *  @sideeffects
 *   @i Prints a message to stderr and aborts the program in case of error.
 *   @i Advances lex state just past the parsed block.
 *   @i Allocates an FbldText* that should be freed when no longer needed.
 */
static FbldText* ParseName(Lex* lex)
{
  FbldLoc loc = lex->loc;

  struct { size_t size; char* xs; } chars;
  FbldInitVector(chars);
  while (IsNameChar(Char(lex))) {
    FbldAppendToVector(chars, Char(lex));
    Advance(lex);
  }
  FbldAppendToVector(chars, '\0');

  FbldText* text = FbldNewText(loc, chars.xs);
  FbldFree(chars.xs);
  return text;
}

/**
 * @func[ParseInlineArgs] Parses a sequence of [...] and {...} args.
 *  There may be zero or more such args.
 *
 *  @arg[Lex*][lex] The lexer state
 *  @arg[FbldMarkupV*][args] Output vector to append parsed args to.
 *  @returns[bool] True on success, false in case of error.
 *  @sideeffects
 *   @i Prints a message to stderr in case of error.
 *   @i Advances lex state just past the parsed args.
 *   @i Appends to args vector.
 */
static bool ParseInlineArgs(Lex* lex, FbldMarkupV* args)
{
  while (Is(lex, "[") || Is(lex, "{")) {
    if (Is(lex, "{")) {
      Advance(lex);
      FbldLoc loc = lex->loc;

      size_t nest = 0;
      struct { size_t size; char* xs; } chars;
      FbldInitVector(chars);
      while (nest > 0 || !Is(lex, "}")) {
        if (IsEnd(lex)) {
          FbldReportError("end of file in literal inline arg\n", lex->loc);
          FbldFree(chars.xs);
          return false;
        }

        if (Is(lex, "{")) {
          nest++;
        }

        if (Is(lex, "}")) {
          nest--;
        }

        FbldAppendToVector(chars, Char(lex));
        Advance(lex);
      }
      Advance(lex);
      FbldAppendToVector(chars, '\0');
      FbldMarkup* arg = FbldAlloc(FbldMarkup);
      arg->tag = FBLD_MARKUP_PLAIN;
      arg->text = FbldNewText(loc, chars.xs);
      arg->refcount = 1;
      FbldFree(chars.xs);
      FbldInitVector(arg->markups);
      FbldAppendToVector(*args, arg);
      continue;
    }

    if (Is(lex, "[")) {
      Advance(lex);
      FbldMarkup* arg = ParseInline(lex, INLINE_ARG);
      if (arg == NULL) {
        return false;
      }
      FbldAppendToVector(*args, arg);

      assert(Is(lex, "]"));
      Advance(lex);
    }
  }
  return true;
}

/**
 * @func[ParseInlineCommand] Parses an inline command.
 *  Starting just after the initial '@'.
 *
 *  @arg[Lex*][lex] The lexer state.
 *  @returns[FbldMarkup*] The parsed inline command, or NULL in case of error.
 *  @sideeffects
 *   @i Prints a message to stderr in case of error.
 *   @i Advances lex state just past the parsed inline command.
 *   @i Allocates an FbldMarkup* that should be freed when no longer needed.
 */
static FbldMarkup* ParseInlineCommand(Lex* lex)
{
  FbldMarkup* markup = FbldAlloc(FbldMarkup);
  markup->tag = FBLD_MARKUP_COMMAND;
  markup->text = ParseName(lex);
  markup->refcount = 1;
  FbldInitVector(markup->markups);

  if (!ParseInlineArgs(lex, &markup->markups)) {
    FbldFreeMarkup(markup);
    return NULL;
  }
  return markup;
}

/**
 * @func[ParseInline] Parses fbld inline structured markup.
 *  @arg[Lex*][lex] The lexer state.
 *  @arg[InlineContext][context] The context to parse from.
 *  @returns[FbldMarkup*] The parsed inline markup, or NULL in case of error.
 *  @sideeffects
 *   @i Prints a message to stderr in case of error.
 *   @i Advances lex state just past the parsed inline text.
 *   @i Allocates an FbldMarkup* that should be freed when no longer needed.
 */
static FbldMarkup* ParseInline(Lex* lex, InlineContext context)
{
  // Inline markup is a sequence of plain text and inline commands.
  FbldMarkup* markup = FbldAlloc(FbldMarkup);
  markup->tag = FBLD_MARKUP_SEQUENCE;
  markup->text = NULL;
  markup->refcount = 1;
  FbldInitVector(markup->markups);

  struct { size_t size; char* xs; } chars;
  FbldInitVector(chars);
  FbldLoc loc = lex->loc;
  while (true) {
    if (context == INLINE_ARG && Is(lex, "]")) {
      break;
    }

    if (context == INLINE_ARG && IsEnd(lex)) {
      FbldReportError("unexpected end of file\n", lex->loc);
      FbldFree(chars.xs);
      FbldFreeMarkup(markup);
      return NULL;
    }

    if (context == SAME_LINE_ARG
        && (Is(lex, "\n") || Is(lex, " @\n") || Is(lex, " @@\n"))) {
      break;
    }

    if (context == IMPLICIT_BLOCK) {
      if (lex->loc.column == 1 && Is(lex, "\n")) {
        break;
      }
      if (IsEnd(lex)) {
        break;
      }
    }

    if (Is(lex, "@")) {
      Advance(lex);

      if (chars.size != 0) {
        FbldAppendToVector(chars, '\0');

        FbldText* text = FbldNewText(loc, chars.xs);
        chars.size = 0;

        FbldMarkup* plain = FbldAlloc(FbldMarkup);
        plain->tag = FBLD_MARKUP_PLAIN;
        plain->text = text;
        plain->refcount = 1;
        FbldInitVector(plain->markups);
        FbldAppendToVector(markup->markups, plain);
      }

      FbldMarkup* command = ParseInlineCommand(lex);
      if (command == NULL) {
        FbldFree(chars.xs);
        FbldFreeMarkup(markup);
        return NULL;
      }

      FbldAppendToVector(markup->markups, command);
      continue;
    }

    if (Is(lex, "\\")) {
      Advance(lex);
      switch (Char(lex)) {
        case '@': FbldAppendToVector(chars, '@'); break;
        case '[': FbldAppendToVector(chars, '['); break;
        case '\\': FbldAppendToVector(chars, '\\'); break;
        case ']': FbldAppendToVector(chars, ']'); break;
        case 'n': FbldAppendToVector(chars, '\n'); break;
        default: {
          FbldReportError("unsupported escape sequence\n", lex->loc);
          FbldFree(chars.xs);
          FbldFreeMarkup(markup);
          return NULL;
        }
      }
      Advance(lex);
      continue;
    }
    
    FbldAppendToVector(chars, Char(lex));
    Advance(lex);
  }

  if (chars.size != 0) {
    FbldAppendToVector(chars, '\0');
    FbldText* text = FbldNewText(loc, chars.xs);

    FbldMarkup* plain = FbldAlloc(FbldMarkup);
    plain->tag = FBLD_MARKUP_PLAIN;
    plain->text = text;
    plain->refcount = 1;
    FbldInitVector(plain->markups);
    FbldAppendToVector(markup->markups, plain);
  }
  FbldFree(chars.xs);

  if (markup->markups.size == 1) {
    FbldMarkup* tmp = markup->markups.xs[0];
    FbldFree(markup->markups.xs);
    FbldFree(markup);
    markup = tmp;
  }

  return markup;
}

/**
 * @func[ParseBlockCommand] Parses an fbld block command.
 *  From just after the initial '@' character.
 *
 *  @arg[Lex*][lex] The lexer state.
 *  @returns[FbldMarkup*] The parsed command, or NULL in case of error.
 *  @sideeffects
 *   @i Prints a message to stderr in case of error.
 *   @i Advances lex state just past the parsed block.
 *   @i Allocates an FbldMarkup* that should be freed when no longer needed.
 */
static FbldMarkup* ParseBlockCommand(Lex* lex)
{
  FbldMarkup* markup = FbldAlloc(FbldMarkup);
  markup->tag = FBLD_MARKUP_COMMAND;
  markup->text = ParseName(lex);
  markup->refcount = 1;
  FbldInitVector(markup->markups);

  while (true) {
    // Inline args.
    if (!ParseInlineArgs(lex, &markup->markups)) {
      FbldFreeMarkup(markup);
      return NULL;
    }

    // Same line arg.
    if (Is(lex, " ") && !Is(lex, " @\n") && !Is(lex, " @@\n")) {
      Advance(lex);
      FbldMarkup* same_line = ParseInline(lex, SAME_LINE_ARG);
      if (same_line == NULL) {
        FbldFreeMarkup(markup);
        return NULL;
      }
      FbldAppendToVector(markup->markups, same_line);
    }

    // Same line final arg.
    if (Is(lex, " @@\n")) {
      Advance(lex); Advance(lex); Advance(lex); Advance(lex);
      FbldMarkup* final = ParseBlock(lex);
      if (final == NULL) {
        FbldFreeMarkup(markup);
        return NULL;
      }
      FbldAppendToVector(markup->markups, final);
      return markup;
    }

    // Next line literal arg.
    if (Is(lex, " @\n")) {
      Advance(lex); Advance(lex); Advance(lex);
      FbldLoc loc = lex->loc;

      lex->indent++;
      struct { size_t size; char* xs; } chars;
      FbldInitVector(chars);
      while (!IsEnd(lex)) {
        FbldAppendToVector(chars, Char(lex));
        Advance(lex);
      }
      lex->indent--;

      // Strip any trailing blank lines.
      while (chars.size > 1
          && chars.xs[chars.size-1] == '\n'
          && chars.xs[chars.size-2] == '\n') {
        chars.size--;
      }

      FbldAppendToVector(chars, '\0');
      FbldMarkup* arg = FbldAlloc(FbldMarkup);
      arg->tag = FBLD_MARKUP_PLAIN;
      arg->text = FbldNewText(loc, chars.xs);
      arg->refcount = 1;
      FbldFree(chars.xs);
      FbldInitVector(arg->markups);
      FbldAppendToVector(markup->markups, arg);
    } else if (Is(lex, "\n")) {
      Advance(lex);
    } else {
      fprintf(stderr, "Got: 0x%x\n", Char(lex));
      FbldReportError("expected newline\n", lex->loc);
      FbldFreeMarkup(markup);
      return NULL;
    }

    // Next line arg.
    if (Is(lex, " ")) {
      lex->indent++;
      FbldMarkup* arg = ParseBlock(lex);
      if (arg == NULL) {
        FbldFreeMarkup(markup);
        return NULL;
      }
      lex->indent--;

      FbldAppendToVector(markup->markups, arg);
    }

    // Next line final arg.
    if (Is(lex, "@@\n")) {
      Advance(lex); Advance(lex); Advance(lex);
      FbldMarkup* final = ParseBlock(lex);
      if (final == NULL) {
        FbldFreeMarkup(markup);
        return NULL;
      }
      FbldAppendToVector(markup->markups, final);
      return markup;
    }

    // Continuation.
    if (Is(lex, "@\n") || Is(lex, "@ ") || Is(lex, "@[") || Is(lex, "@{")) {
      Advance(lex);
      continue;
    }

    return markup;
  }

  assert(false && "unreachable");
  return NULL;
}

/**
 * @func[ParseBlock] Parses fbld block structured markup.
 *  @arg[Lex*][lex] The lexer state.
 *  @returns[FbldMarkup*] The parsed block markup.
 *  @sideeffects
 *   @i Prints a message to stderr in case of error.
 *   @i Advances lex state just past the parsed block.
 *   @i Allocates an FbldMarkup* that should be freed when no longer needed.
 */
static FbldMarkup* ParseBlock(Lex* lex)
{
  FbldMarkup* markup = FbldAlloc(FbldMarkup);
  markup->tag = FBLD_MARKUP_SEQUENCE;
  markup->text = NULL;
  markup->refcount = 1;
  FbldInitVector(markup->markups);

  // Skip blank lines.
  while (Is(lex, "\n")) Advance(lex);

  while (!IsEnd(lex)) {
    // Explicit implicit block command.
    if (Is(lex, "@@")) {
      Advance(lex);
      FbldMarkup* cmd = FbldAlloc(FbldMarkup);
      cmd->tag = FBLD_MARKUP_COMMAND;
      cmd->text = FbldNewText(lex->loc, ".block");
      cmd->refcount = 1;
      FbldInitVector(cmd->markups);

      FbldMarkup* arg = ParseInline(lex, IMPLICIT_BLOCK);
      if (arg == NULL) {
        FbldFreeMarkup(cmd);
        FbldFreeMarkup(markup);
        return NULL;
      }

      FbldAppendToVector(cmd->markups, arg);
      FbldAppendToVector(markup->markups, cmd);

      // Skip blank lines.
      while (Is(lex, "\n")) Advance(lex);
      continue;
    }

    // Block Command.
    if (Is(lex, "@")) {
      Advance(lex);
      FbldMarkup* command = ParseBlockCommand(lex);
      if (command == NULL) {
        FbldFreeMarkup(markup);
        return NULL;
      }
      FbldAppendToVector(markup->markups, command);

      // Skip blank lines.
      while (Is(lex, "\n")) Advance(lex);
      continue;
    }

    // Implicit block
    FbldMarkup* cmd = FbldAlloc(FbldMarkup);
    cmd->tag = FBLD_MARKUP_COMMAND;
    cmd->text = FbldNewText(lex->loc, ".block");
    cmd->refcount = 1;
    FbldInitVector(cmd->markups);

    FbldMarkup* arg = ParseInline(lex, IMPLICIT_BLOCK);
    if (arg == NULL) {
      FbldFreeMarkup(cmd);
      FbldFreeMarkup(markup);
      return NULL;
    }

    FbldAppendToVector(cmd->markups, arg);
    FbldAppendToVector(markup->markups, cmd);

    // Skip blank lines.
    while (Is(lex, "\n")) Advance(lex);
  }

  return markup;
}

// See documentation in fbld.h
FbldMarkup* FbldParse(const char** inputs)
{
  Lex lex = {
    .inputs = inputs,
    .fin = NULL,
    .loc = { .file = "???", .line = 0, .column = 0 },
    .indent = 0,
    .next = FbldAllocArray(char, 4),
    .next_size = 0,
    .next_capacity = 4,
  };

  FbldMarkup* parsed = ParseBlock(&lex);

  if (lex.fin != NULL) {
    fclose(lex.fin);
  }

  FbldFree(lex.next);
  return parsed;
}
