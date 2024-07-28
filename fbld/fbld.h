/**
 * @file fbld.h
 *  General header file for fbld types and functions.
 */

#ifndef _FBLD_H_
#define _FBLD_H_

#include <stddef.h>     // for size_t

typedef struct FbldMarkup FbldMarkup;

typedef struct {
  const char* file;
  size_t line;
  size_t column;
} FbldLoc;

typedef struct {
  FbldLoc loc;
  char str[];
} FbldText;

typedef struct {
  size_t size;
  FbldText** xs;
} FbldTextV;

/**
 * @func[FbldNewText] Allocate a new FbldText.
 *  @arg[FbldLoc][loc] The location to use for the text.
 *  @arg[const char*][str] The string contents. Borrowed.
 *  @returns[FbldText*] Newly allocated FbldText
 *  @sideeffects
 *   Allocates new FbldText that should be freed when no longer needed.
 */
FbldText* FbldNewText(FbldLoc loc, const char* str);

typedef struct {
  size_t size;
  FbldMarkup** xs;
} FbldMarkupV;

typedef enum {
  FBLD_MARKUP_PLAIN,
  FBLD_MARKUP_COMMAND,
  FBLD_MARKUP_SEQUENCE
} FbldMarkupTag;

struct FbldMarkup {
  FbldMarkupTag tag;
  FbldText* text;         // Plain text, command name, or NULL.
  FbldMarkupV markups;    // Sequence of markups, command args, or empty.
};

/**
 * @func[FbldFreeMarkup] Frees resources associated with the given markup.
 *  @arg[FbldMarkup*][markup] The markup to free resources for.
 *
 *  @sideeffects
 *   Frees resources associated with the given markup.
 */
void FbldFreeMarkup(FbldMarkup* markup);

/**
 * @func[FbldCopyMarkup] Makes a copy of the given markup.
 *  @arg[FbldMarkup*][markup] The markup to copy.
 *  @returns[FbldMarkup*] A copy of the markup.
 *  @sideeffects
 *   Allocates an FbldMarkup* that should be freed with FbldFreeMarkup.
 */
FbldMarkup* FbldCopyMarkup(FbldMarkup* markup);

/**
 * @func[FbldError] Reports an error.
 *  @arg[FbldLoc][loc] The location of the error.
 *  @arg[const char*][msg] The error message.
 *  @sideeffects
 *   Prints the error message to stderr and aborts the program.
 */
void FbldError(FbldLoc loc, const char* message);

/**
 * @func[FbldParse] Parses a sequence of fbld files.
 *  @arg[const char**][inputs] Null terminated array of input file names.
 *
 *  @returns[FbldMarkup*] The parsed markup.
 *
 *  @sideeffects
 *   @i Prints an error to stderr and aborts the program in case of error.
 *   @item
 *    References the input file names in the parsed markup. The caller is
 *    responsible for ensuring those pointers stay valid as long as they may
 *    continue to be access from the parsed markup.
 *   @item
 *    The user is responsible for calling FbldFreeMarkup on the returned
 *    markup object when no longer needed.
 */
FbldMarkup* FbldParse(const char** inputs);

/**
 * @func[FbldEval] Evaluates an fbld document.
 *  @arg[FbldMarkup*][markup] The markup to evaluate.
 *
 *  @returns[FbldMarkup*] The evaluated markup.
 *
 *  @sideeffects
 *   @i Prints an error to stderr and aborts the program in case of error.
 *   @item
 *    The user is responsible for calling FbldFreeMarkup on the returned
 *    markup object when no longer needed.
 */
FbldMarkup* FbldEval(FbldMarkup* markup);

/**
 * @func[FbldPrintMarkup] Prints markup to stdout.
 *  @arg[FbldMarkup*][markup] The markup to print.
 *
 *  @sideeffects
 *   @i Outputs the markup to stdout.
 *   @i Prints an error to stderr and aborst the program in case of error.
 */
void FbldPrintMarkup(FbldMarkup* markup);

/**
 * @func[FbldDebugMarkup] Debug prints markup to stdout.
 *  @arg[FbldMarkup*][markup] The markup to print.
 *  @sideeffects Outputs the markup to stdout.
 */
void FbldDebugMarkup(FbldMarkup* markup);

#endif // _FBLD_H_
