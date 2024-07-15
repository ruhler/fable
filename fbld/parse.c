
#include <assert.h>   // for assert
#include <ctype.h>    // for isalnum
#include <stdbool.h>  // for false
#include <stdio.h>    // for FILE, feof, etc.
#include <stdlib.h>   // for abort, malloc
#include <string.h>   // for strcpy

#include "fbld.h"

/**
 * @struct[Lex] State of the lexer.
 *  @field[const char**][inputs] Remaining input files to process.
 *  @field[FILE*][fin] The current input file being processed.
 *  @field[int][c] The next input character, or EOF for end of file.
 *  @field[FbldLoc][loc] The current location.
 *  @field[char*][buffer] A scratch buffer for reading in characters.
 *  @field[size_t][buffer_size] The number of characters in the buffer.
 *  @field[size_t][buffer_capacity] The allocated size of the buffer.
 */
typedef struct {
  const char** inputs;
  FILE* fin;
  int c;
  FbldLoc loc;

  char* buffer;
  size_t buffer_size;
  size_t buffer_capacity;
} Lex;

// The context we are parsing inline text from.
//  INLINE_ARG - Inside a [...] block.
//  SAME_LINE_ARG - From a same line arg.
typedef enum {
  INLINE_ARG,
  SAME_LINE_ARG
} InlineContext;

// The terminator of a same line arg.
typedef enum {
  END_SAME_LINE,
  NEXT_LINE_LITERAL,
  NEXT_LINE_FINAL
} SameLineArgEnd;

static void Next(Lex* lex);

static void ClearBuffer(Lex* lex);
static void AddToBuffer(Lex* lex, char c);

static bool IsNameChar(int c);
static FbldText* ParseName(Lex* lex);
static void ParseInlineArgs(Lex* lex, FbldMarkupV* args);
static FbldMarkup* ParseInlineCommand(Lex* lex);
static FbldMarkup* ParseInline(Lex* lex, InlineContext context, SameLineArgEnd* same_line_end);
static FbldMarkup* ParseBlockCommand(Lex* lex);
static FbldMarkup* ParseBlock(Lex* lex);

/**
 * @func[Next] Advance the lexer by a single character.
 *  @arg[Lex*][lex] The lexer state to advance.
 *  @sideeffects Advances to the next character in the input.
 */
static void Next(Lex* lex)
{
  // Advance the location.
  switch (lex->c) {
    case '\n':
      lex->loc.line++;
      lex->loc.column = 1;
      break;

    case EOF:
      break;

    default:
      lex->loc.column++;
  }

  lex->c = EOF;
  while (lex->c == EOF) {
    if (lex->fin == NULL) {
      const char* filename = *lex->inputs++;
      if (filename == NULL) {
        // We've finished processing all the inputs.
        return;
      }

      // Open the next input file for processing.
      lex->fin = fopen(filename, "r");
      if (lex->fin == NULL) {
        perror("fopen");
        abort();
      }
      lex->loc.file = filename;
      lex->loc.line = 1;
      lex->loc.column = 1;
    }

    lex->c = fgetc(lex->fin);
  }
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
  if (lex->buffer_size < lex->buffer_capacity) {
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
  FbldText* text = malloc(sizeof(FbldText));
  text->loc = lex->loc;

  ClearBuffer(lex);
  while (IsNameChar(lex->c)) {
    AddToBuffer(lex, (char)lex->c);
    Next(lex);
  }
  AddToBuffer(lex, '\0');

  FbldString* str = malloc(sizeof(FbldString) + lex->buffer_size * sizeof(char));
  str->refcount = 1;
  strcpy(str->str, lex->buffer);

  text->str = str;
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
  while (lex->c == '[' || lex->c == '{') {
    if (lex->c == '{') {
      assert(false && "TODO: Parse literal inline arg");
      return;
    }

    if (lex->c == '[') {
      Next(lex);
      FbldMarkup* arg = ParseInline(lex, INLINE_ARG, NULL);
      FbldAppendToVector(args, arg);
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
  FbldInitVector(&markup->markups);

  ParseInlineArgs(lex, &markup->markups);
  return markup;
}

/**
 * @func[ParseInline] Parses fbld inline structured markup.
 *  In the case of INLINE_ARG, parses just past the closing ']' character. In
 *  the case of SAME_LINE_ARG, parses just past the closing '\n' and sets
 *  same_line_end appropriately. same_line_end must be provided in case of
 *  SAME_LINE_ARG; it can be NULL in case of INLINE_ARG.
 *
 *  @arg[Lex*][lex] The lexer state.
 *  @arg[InlineContext][context] The context to parse from.
 *  @arg[SameLineArgEnd*][same_line_end] How a same line arg ended.
 *  @returns[FbldMarkup*] The parsed inline markup.
 *  @sideeffects
 *   @i Prints a message to stderr and aborts the program in case of error.
 *   @i Advances lex state just past the parsed inline text.
 *   @i Allocates an FbldMarkup* that should be freed when no longer needed.
 */
static FbldMarkup* ParseInline(Lex* lex, InlineContext context, SameLineArgEnd* same_line_end)
{
  // Inline markup is a sequence of plain text and inline commands.
  FbldMarkup* markup = malloc(sizeof(FbldMarkup));
  markup->tag = FBLD_MARKUP_SEQUENCE;
  markup->text = NULL;
  FbldInitVector(&markup->markups);

  ClearBuffer(lex);
  FbldLoc loc = lex->loc;
  while (true) {
    if (context == INLINE_ARG && lex->c == ']') {
      Next(lex);
      break;
    }

    if (context == SAME_LINE_ARG && lex->c == '\n') {
      *same_line_end = END_SAME_LINE;
      Next(lex);
      break;
    }

    if (lex->c == '@') {
      Next(lex);

      bool prior_space = lex->buffer_size > 0
        && lex->buffer[lex->buffer_size-1] == ' ';
      if (context == SAME_LINE_ARG && prior_space && lex->c == '\n') {
        // Next line literal case: ... @\n
        lex->buffer_size--;
        *same_line_end = NEXT_LINE_LITERAL;
        Next(lex);
        break;
      }

      if (context == SAME_LINE_ARG && prior_space && lex->c == '@') {
        // The only valid thing for ' @@' is if it ends in \n and becomes a
        // next line final arg ' @@\n'.
        Next(lex);
        if (lex->c != '\n') {
          FbldError(lex->loc, "expected newline after @@");
        }
        lex->buffer_size--;
        Next(lex);
        break;
      }

      if (lex->buffer_size != 0) {
        AddToBuffer(lex, '\0');
        FbldString* str = malloc(sizeof(FbldString) + lex->buffer_size * sizeof(char));
        str->refcount = 1;
        strcpy(str->str, lex->buffer);

        FbldText* text = malloc(sizeof(FbldText));
        text->loc = loc;
        text->str = str;

        FbldMarkup* plain = malloc(sizeof(FbldMarkup));
        plain->tag = FBLD_MARKUP_PLAIN;
        plain->text = text;
        FbldInitVector(&plain->markups);
        FbldAppendToVector(&markup->markups, plain);

        ClearBuffer(lex);
      }

      FbldMarkup* command = ParseInlineCommand(lex);
      FbldAppendToVector(&markup->markups, command);
      continue;
    }

    if (lex->c == '\\') {
      Next(lex);
      switch (lex->c) {
        case '@': AddToBuffer(lex, '@'); break;
        case '[': AddToBuffer(lex, '['); break;
        case '\\': AddToBuffer(lex, '\\'); break;
        case ']': AddToBuffer(lex, ']'); break;
        case 'n': AddToBuffer(lex, '\n'); break;
        default: FbldError(lex->loc, "unsupported escape sequence"); break;
      }
      continue;
    }
    
    AddToBuffer(lex, lex->c);
    Next(lex);
  }

  if (lex->buffer_size != 0) {
    AddToBuffer(lex, '\0');
    FbldString* str = malloc(sizeof(FbldString) + lex->buffer_size * sizeof(char));
    str->refcount = 1;
    strcpy(str->str, lex->buffer);

    FbldText* text = malloc(sizeof(FbldText));
    text->loc = loc;
    text->str = str;

    FbldMarkup* plain = malloc(sizeof(FbldMarkup));
    plain->tag = FBLD_MARKUP_PLAIN;
    plain->text = text;
    FbldInitVector(&plain->markups);
    FbldAppendToVector(&markup->markups, plain);
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
  FbldInitVector(&markup->markups);

  ParseInlineArgs(lex, &markup->markups);

  SameLineArgEnd same_line_end = END_SAME_LINE;
  if (lex->c == ' ') {
    Next(lex);
    FbldMarkup* same_line = ParseInline(lex, SAME_LINE_ARG, &same_line_end);
    FbldAppendToVector(&markup->markups, same_line);
  } else if (lex->c != '\n') {
    FbldError(lex->loc, "expecting space or newline");
  }

  assert(same_line_end != NEXT_LINE_LITERAL && "TODO: next line literal");
  assert(same_line_end != NEXT_LINE_FINAL && "TODO: next line final");
  return markup;
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
  FbldInitVector(&markup->markups);

  while (lex->c == '@') {
    Next(lex);
    FbldMarkup* command = ParseBlockCommand(lex);
    FbldAppendToVector(&markup->markups, command);
  }

  if (lex->c != EOF) {
    FbldError(lex->loc, "expected '@' or EOF");
  }
  return markup;
}

FbldMarkup* FbldParse(const char** inputs)
{
  Lex lex = {
    .inputs = inputs,
    .fin = NULL,
    .c = ' ',
    .loc = { .file = "???", .line = 0, .column = 0 },
    .buffer = malloc(4 * sizeof(char)),
    .buffer_size = 0,
    .buffer_capacity = 4,
  };
  Next(&lex);

  FbldMarkup* parsed = ParseBlock(&lex);
  free(lex.buffer);
  return parsed;
}
