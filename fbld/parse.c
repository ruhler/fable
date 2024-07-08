
#include <assert.h>   // for assert
#include <stdbool.h>  // for false
#include <stdio.h>    // for FILE, feof, etc.
#include <stdlib.h>   // for abort, malloc

#include "fbld.h"

/**
 * @struct[Lex] State of the lexer.
 *  @field[const char**][inputs] Remaining input files to process.
 *  @field[FILE*][fin] The current input file being processed.
 *  @field[int][c] The next input character, or EOF for end of file.
 *  @field[FbldLoc][loc] The current location.
 */
typedef struct {
  const char** inputs;
  FILE* fin;
  int c;
  FbldLoc loc;
} Lex;

static void Next(Lex* lex);
static FbldText* ParseName(Lex* lex);
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
  assert(false && "TODO: ParseName");
  return NULL;
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

  assert(false && "TODO: Parse block command args");

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
  };
  Next(&lex);
  return ParseBlock(&lex);
}
