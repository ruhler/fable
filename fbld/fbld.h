/**
 * @file fbld.h
 *  General header file for fbld types and functions.
 */

#ifndef _FBLD_H_
#define _FBLD_H_

#include <stdbool.h>    // for bool
#include <stddef.h>     // for size_t


/**
 * @struct[FbldLoc] Source file location
 *  @field[FbleString*][source]
 *   Source file name. Assumed to be owned by someone else.
 *  @field[size_t][line] The line number, starting at 1.
 *  @field[size_t][column] The column number, starting at 1.
 */
typedef struct {
  const char* file;
  size_t line;
  size_t column;
} FbldLoc;

/**
 * @func[FbldReportError] Reports an error.
 *  Reports an error message associated with a location in a source file.
 *
 *  This uses a printf-like format string. The following format specifiers are
 *  supported:
 *
 *  @code[txt] @
 *   %i - size_t
 *   %s - const char*
 *   %% - literal '%'
 *
 *  Please add additional format specifiers as needed.
 *
 *  @arg[const char*] format
 *   A printf format string for the error message.
 *  @arg[FbldLoc] loc
 *   The location of the error message to report.
 *  @arg[...] 
 *   Format arguments as specified by the format string.
 *
 *  @sideeffects
 *   Prints an error message to stderr with error location.
 */
void FbldReportError(const char* format, FbldLoc loc, ...);

/*
 * @struct[FbldText] A string with a location.
 *  @field[FbldLoc][loc] The source location of the string.
 *  @field[char*][str] The contents of the string.
 */
typedef struct {
  FbldLoc loc;
  char str[];
} FbldText;

/**
 * @struct[FbldTextV] Vector of FbldText.
 *  @field[size_t][size] The number of elements.
 *  @field[FbldText**][xs] The elements.
 */
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

/**
 * Forward declaration for FbldMarkup.
 */
typedef struct FbldMarkup FbldMarkup;

/**
 * @struct[FbldMarkupV] A vector of FbldMarkup.
 *  @field[size_t][size] Number of elements.
 *  @field[FbldMarkup**][xs] The Elements.
 */
typedef struct {
  size_t size;
  FbldMarkup** xs;
} FbldMarkupV;

/**
 * Different kinds of FbldMarkup.
 */
typedef enum {
  FBLD_MARKUP_PLAIN,
  FBLD_MARKUP_COMMAND,
  FBLD_MARKUP_SEQUENCE
} FbldMarkupTag;

/**
 * @struct[FbldMarkup] Abstract syntax for fbld document.
 *  @field[FbldMarkupTag][tag] The type of markup.
 *  @field[FbldText*][text] Plain text, command name, or NULL.
 *  @field[FbldMarkupV][markups] Sequence of markups, command args, or empty.
 *  @field[size_t][refcount] Number of references to this markup.
 */
struct FbldMarkup {
  FbldMarkupTag tag;
  FbldText* text;
  FbldMarkupV markups;
  size_t refcount;
};

/**
 * @func[FbldNewPlainMarkup] Allocates plain markup.
 *  @arg[FbldLoc][loc] The location of the markup.
 *  @arg[const char*][text] The text of the markup. Borrowed.
 *  @returns[FbldMarkup*] The newly allocated markup.
 *  @sideeffects
 *   Allocates markup that should be freed with FbldFreeMarkup when no longer
 *   needed.
 */
FbldMarkup* FbldNewPlainMarkup(FbldLoc loc, const char* text);

/**
 * @func[FbldFreeMarkup] Frees resources associated with the given markup.
 *  @arg[FbldMarkup*][markup] The markup to free resources for. May be NULL.
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
 * @func[FbldMarkupLoc] Returns the location of the markup.
 *  @arg[FbldMarkup*][markup] The markup to get the location from.
 *  @returns[FbldLoc] The location of the markup.
 *  @sideeffects None
 */
FbldLoc FbldMarkupLoc(FbldMarkup* markup);

/**
 * @func[FbldTextOfMarkup] Converts markup to text.
 *  @arg[FbldMarkup*][markup] The markup to convert.
 *  @returns[FbldText*] Text version of the markup, or NULL in case of error.
 *  @sideeffects
 *   @i Allocates an FbldText* that should be freed when no longer needed.
 *   @i Prints message to stderr if markup contains unevaluated commands.
 */
FbldText* FbldTextOfMarkup(FbldMarkup* markup);

/**
 * @func[FbldPrintMarkup] Prints markup to stdout.
 *  @arg[FbldMarkup*][markup] The markup to print.
 *  @return[bool] True on success, false in case of error.
 *
 *  @sideeffects
 *   @i Outputs the markup to stdout.
 *   @i Prints an error to stderr in case of error.
 */
bool FbldPrintMarkup(FbldMarkup* markup);

/**
 * @func[FbldDebugMarkup] Debug prints markup to stdout.
 *  @arg[FbldMarkup*][markup] The markup to print.
 *  @sideeffects Outputs the markup to stdout.
 */
void FbldDebugMarkup(FbldMarkup* markup);

/**
 * @func[FbldParse] Parses a sequence of fbld files.
 *  @arg[const char**][inputs] Null terminated array of input file names.
 *
 *  @returns[FbldMarkup*] The parsed markup, or NULL in case of error.
 *
 *  @sideeffects
 *   @i Prints an error to stderr in case of error.
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
 *  @arg[FbldMarkup*][markup] The markup to evaluate. Borrowed.
 *
 *  @returns[FbldMarkup*] The evaluated markup, or NULL in case of error.
 *
 *  @sideeffects
 *   @i Prints an error to stderr in case of error.
 *   @item
 *    The user is responsible for calling FbldFreeMarkup on the returned
 *    markup object when no longer needed.
 */
FbldMarkup* FbldEval(FbldMarkup* markup);

#endif // _FBLD_H_
