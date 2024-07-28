
#include <assert.h>   // for assert
#include <ctype.h>    // for isalnum
#include <stdbool.h>  // for false
#include <stdio.h>    // for FILE, feof, etc.
#include <stdlib.h>   // for abort, malloc
#include <string.h>   // for strcpy

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
 *  @field[char*][next] A buffer for storing the next characters in the input.
 *  @field[size_t][next_size] The number of characters in next.
 *  @field[size_t][next_capacity] The allocated size of next.
 *  @field[char*][buffer] A scratch buffer for reading in characters.
 *  @field[size_t][buffer_size] The number of characters in the buffer.
 *  @field[size_t][buffer_capacity] The allocated size of the buffer.
 */
typedef struct {
  const char** inputs;
  FILE* fin;
  FbldLoc loc;

  char* next;
  size_t next_size;
  size_t next_capacity;

  char* buffer;
  size_t buffer_size;
  size_t buffer_capacity;
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
static char Char(Lex* lex);
static bool Is(Lex* lex, const char* str);
static bool IsEnd(Lex* lex);
static void Advance(Lex* lex);

static void ClearBuffer(Lex* lex);
static void AddToBuffer(Lex* lex, char c);

static bool IsNameChar(int c);
static FbldText* ParseName(Lex* lex);
static void ParseInlineArgs(Lex* lex, FbldMarkupV* args);
static FbldMarkup* ParseInlineCommand(Lex* lex);
static FbldMarkup* ParseInline(Lex* lex, InlineContext context);
static FbldMarkup* ParseBlockCommand(Lex* lex);
static FbldMarkup* ParseBlock(Lex* lex);

/**
 * @func[GetC] Fetches another input character.
 *  Does not cross input files unless current next buffer is empty.
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

    // Open the next input file for processing.
    lex->fin = fopen(filename, "r");
    if (lex->fin == NULL) {
      perror("fopen");
      abort();
    }
    lex->loc.file = filename;
    lex->loc.line = 1;
    lex->loc.column = 1;

    c = fgetc(lex->fin);
  }
  return c == EOF ? END : c;
}

/**
 * @func[Char] Returns the next input character.
 *  @arg[Lex*][lex] The lexer state.
 *  @returns[char] The next input character, or END in case of end of input.
 *  @sideeffects Fetches the next input character if needed.
 */
static char Char(Lex* lex)
{
  if (lex->next_size == 0) {
    char c = GetC(lex);
    if (c == END) {
      return c;
    }

    lex->next[0] = c;
    lex->next_size++;
  }
  return lex->next[0];
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
  size_t len = strlen(str);
  while (lex->next_size < len) {
    char c = GetC(lex);
    if (c == END) {
      break;
    }

    if (lex->next_size == lex->next_capacity) {
      lex->next_capacity *= 2;
      lex->next = realloc(lex->next, lex->next_capacity * sizeof(char));
    }

    lex->next[lex->next_size] = c;
    lex->next_size++;
  }
  return lex->next_size >= len && strncmp(str, lex->next, len) == 0;
}

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
  // We assume we at least tried to read the next input character before
  // advancing past it.
  assert(lex->next_size > 0);

  // Advance the location.
  switch (lex->next[0]) {
    case '\n':
      lex->loc.line++;
      lex->loc.column = 1;
      break;

    default:
      lex->loc.column++;
  }

  lex->next_size--;
  memmove(lex->next, lex->next + 1, lex->next_size);
}

/**
 * @func[ClearBuffer] Clears the lex buffer.
 *  @arg[Lex*][lex] The lexer state.
 *  @sideeffects Resets the size of the lex buffer to zero.
 */
static void ClearBuffer(Lex* lex)
{
  lex->buffer_size = 0;
}

/**
 * @func[AddToBuffer] Add a character to the lex buffer.
 *  @arg[Lex*][lex] The lexer state.
 *  @arg[char][c] The character to add.
 *  @sideeffects
 *   @i Appends the character to the lex buffer, incrementing the size.
 *   @i Reallocs the lex buffer as needed to make room.
 */
static void AddToBuffer(Lex* lex, char c)
{
  if (lex->buffer_size == lex->buffer_capacity) {
    lex->buffer_capacity *= 2;
    lex->buffer = realloc(lex->buffer, lex->buffer_capacity * sizeof(char));
  }
  lex->buffer[lex->buffer_size++] = c;
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

  ClearBuffer(lex);
  while (IsNameChar(Char(lex))) {
    AddToBuffer(lex, Char(lex));
    Advance(lex);
  }
  AddToBuffer(lex, '\0');

  FbldText* text = FbldNewText(loc, lex->buffer);
  ClearBuffer(lex);
  return text;
}

/**
 * @func[ParseInlineArgs] Parses a sequence of [...] and {...} args.
 *  There may be zero or more such args.
 *
 *  @arg[Lex*][lex] The lexer state
 *  @arg[FbldMarkupV*][args] Output vector to append parsed args to.
 *  @sideeffects
 *   @i Prints a message to stderr and aborts the program in case of error.
 *   @i Advances lex state just past the parsed args.
 *   @i Appends to args vector.
 */
static void ParseInlineArgs(Lex* lex, FbldMarkupV* args)
{
  while (Is(lex, "[") || Is(lex, "{")) {
    if (Is(lex, "{")) {
      Advance(lex);
      FbldLoc loc = lex->loc;

      size_t nest = 0;
      ClearBuffer(lex);
      while (nest > 0 || !Is(lex, "}")) {
        if (IsEnd(lex)) {
          FbldError(lex->loc, "end of file in literal inline arg");
        }

        if (Is(lex, "{")) {
          nest++;
        }

        if (Is(lex, "}")) {
          nest--;
        }

        AddToBuffer(lex, Char(lex));
        Advance(lex);
      }
      Advance(lex);
      AddToBuffer(lex, '\0');
      FbldMarkup* arg = malloc(sizeof(FbldMarkup));
      arg->tag = FBLD_MARKUP_PLAIN;
      arg->text = FbldNewText(loc, lex->buffer);
      ClearBuffer(lex);
      FbldInitVector(arg->markups);
      FbldAppendToVector(*args, arg);
      continue;
    }

    if (Is(lex, "[")) {
      Advance(lex);
      FbldMarkup* arg = ParseInline(lex, INLINE_ARG);
      FbldAppendToVector(*args, arg);

      assert(Is(lex, "]"));
      Advance(lex);
    }
  }
}

/**
 * @func[ParseInlineCommand] Parses an inline command.
 *  Starting just after the initial '@'.
 *
 *  @arg[Lex*][lex] The lexer state.
 *  @returns[FbldMarkup*] The parsed inline command.
 *  @sideeffects
 *   @i Prints a message to stderr and aborts the program in case of error.
 *   @i Advances lex state just past the parsed inline command.
 *   @i Allocates an FbldMarkup* that should be freed when no longer needed.
 */
static FbldMarkup* ParseInlineCommand(Lex* lex)
{
  FbldMarkup* markup = malloc(sizeof(FbldMarkup));
  markup->tag = FBLD_MARKUP_COMMAND;
  markup->text = ParseName(lex);
  FbldInitVector(markup->markups);

  ParseInlineArgs(lex, &markup->markups);
  return markup;
}

/**
 * @func[ParseInline] Parses fbld inline structured markup.
 *  @arg[Lex*][lex] The lexer state.
 *  @arg[InlineContext][context] The context to parse from.
 *  @returns[FbldMarkup*] The parsed inline markup.
 *  @sideeffects
 *   @i Prints a message to stderr and aborts the program in case of error.
 *   @i Advances lex state just past the parsed inline text.
 *   @i Allocates an FbldMarkup* that should be freed when no longer needed.
 */
static FbldMarkup* ParseInline(Lex* lex, InlineContext context)
{
  // Inline markup is a sequence of plain text and inline commands.
  FbldMarkup* markup = malloc(sizeof(FbldMarkup));
  markup->tag = FBLD_MARKUP_SEQUENCE;
  markup->text = NULL;
  FbldInitVector(markup->markups);

  ClearBuffer(lex);
  FbldLoc loc = lex->loc;
  while (true) {
    if (context == INLINE_ARG && Is(lex, "]")) {
      break;
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

      if (lex->buffer_size != 0) {
        AddToBuffer(lex, '\0');

        FbldText* text = FbldNewText(loc, lex->buffer);
        ClearBuffer(lex);

        FbldMarkup* plain = malloc(sizeof(FbldMarkup));
        plain->tag = FBLD_MARKUP_PLAIN;
        plain->text = text;
        FbldInitVector(plain->markups);
        FbldAppendToVector(markup->markups, plain);
      }

      FbldMarkup* command = ParseInlineCommand(lex);
      FbldAppendToVector(markup->markups, command);
      continue;
    }

    if (Is(lex, "\\")) {
      Advance(lex);
      switch (Char(lex)) {
        case '@': AddToBuffer(lex, '@'); break;
        case '[': AddToBuffer(lex, '['); break;
        case '\\': AddToBuffer(lex, '\\'); break;
        case ']': AddToBuffer(lex, ']'); break;
        case 'n': AddToBuffer(lex, '\n'); break;
        default: FbldError(lex->loc, "unsupported escape sequence"); break;
      }
      Advance(lex);
      continue;
    }
    
    AddToBuffer(lex, Char(lex));
    Advance(lex);
  }

  if (lex->buffer_size != 0) {
    AddToBuffer(lex, '\0');
    FbldText* text = FbldNewText(loc, lex->buffer);
    ClearBuffer(lex);

    FbldMarkup* plain = malloc(sizeof(FbldMarkup));
    plain->tag = FBLD_MARKUP_PLAIN;
    plain->text = text;
    FbldInitVector(plain->markups);
    FbldAppendToVector(markup->markups, plain);
  }

  if (markup->markups.size == 1) {
    FbldMarkup* tmp = markup->markups.xs[0];
    free(markup->markups.xs);
    free(markup);
    markup = tmp;
  }

  return markup;
}

/**
 * @func[ParseBlockCommand] Parses an fbld block command.
 *  From just after the initial '@' character.
 *
 *  @arg[Lex*][lex] The lexer state.
 *  @returns[FbldMarkup*] The parsed command.
 *  @sideeffects
 *   @i Prints a message to stderr and aborts the program in case of error.
 *   @i Advances lex state just past the parsed block.
 *   @i Allocates an FbldMarkup* that should be freed when no longer needed.
 */
static FbldMarkup* ParseBlockCommand(Lex* lex)
{
  FbldMarkup* markup = malloc(sizeof(FbldMarkup));
  markup->tag = FBLD_MARKUP_COMMAND;
  markup->text = ParseName(lex);
  FbldInitVector(markup->markups);

  ParseInlineArgs(lex, &markup->markups);

  if (Is(lex, " ") && !Is(lex, " @\n") && !Is(lex, " @@\n")) {
    Advance(lex);
    FbldMarkup* same_line = ParseInline(lex, SAME_LINE_ARG);
    FbldAppendToVector(markup->markups, same_line);
  }
  
  if (Is(lex, "\n")) {
    return markup;
  }

  if (Is(lex, " @\n")) {
    assert(false && "TODO: next line literal");
    return NULL;
  }

  if (Is(lex, " @@\n")) {
    Advance(lex); Advance(lex); Advance(lex); Advance(lex);
    FbldMarkup* final = ParseBlock(lex);
    FbldAppendToVector(markup->markups, final);
    return markup;
  }

  return NULL;
}

/**
 * @func[ParseBlock] Parses fbld block structured markup.
 *  @arg[Lex*][lex] The lexer state.
 *  @returns[FbldMarkup*] The parsed block markup.
 *  @sideeffects
 *   @i Prints a message to stderr and aborts the program in case of error.
 *   @i Advances lex state just past the parsed block.
 *   @i Allocates an FbldMarkup* that should be freed when no longer needed.
 */
static FbldMarkup* ParseBlock(Lex* lex)
{
  FbldMarkup* markup = malloc(sizeof(FbldMarkup));
  markup->tag = FBLD_MARKUP_SEQUENCE;
  markup->text = NULL;
  FbldInitVector(markup->markups);

  // Skip blank lines.
  while (Is(lex, "\n")) Advance(lex);

  while (!IsEnd(lex)) {
    // Command.
    if (Is(lex, "@")) {
      Advance(lex);
      FbldMarkup* command = ParseBlockCommand(lex);
      FbldAppendToVector(markup->markups, command);

      // Skip blank lines.
      while (Is(lex, "\n")) Advance(lex);
      continue;
    }

    // Implicit block
    FbldMarkup* cmd = malloc(sizeof(FbldMarkup));
    cmd->tag = FBLD_MARKUP_COMMAND;
    cmd->text = FbldNewText(lex->loc, ".block");
    FbldInitVector(cmd->markups);
    FbldAppendToVector(cmd->markups, ParseInline(lex, IMPLICIT_BLOCK));
    FbldAppendToVector(markup->markups, cmd);

    // Skip blank lines.
    while (Is(lex, "\n")) Advance(lex);
  }

  return markup;
}

FbldMarkup* FbldParse(const char** inputs)
{
  Lex lex = {
    .inputs = inputs,
    .fin = NULL,
    .loc = { .file = "???", .line = 0, .column = 0 },
    .next = malloc(4 * sizeof(char)),
    .next_size = 0,
    .next_capacity = 4,
    .buffer = malloc(4 * sizeof(char)),
    .buffer_size = 0,
    .buffer_capacity = 4,
  };

  FbldMarkup* parsed = ParseBlock(&lex);
  free(lex.buffer);
  return parsed;
}
