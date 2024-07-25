/**
 * @file fbld.h
 *  General header file for fbld types and functions.
 */

#ifndef _FBLD_H_
#define _FBLD_H_

#include <stddef.h>     // for size_t

typedef struct FbldMarkup FbldMarkup;

/**
 * Magic number used in FbldString.
 */
typedef enum {
  FBLD_STRING_MAGIC = 0x51617a
} FbldStringMagic;

/**
 * @struct[FbldString] A reference counted string of characters.
 *  Pass by pointer. Explicit copy and free required.
 *
 *  Note: The magic field is set to FBLE_STRING_MAGIC and is used to detect
 *  double frees of FbleString, which we have had trouble with in the past.
 *
 *  @field[size_t][refcount] The reference count.
 *  @field[FbleStringMagic][magic] FBLE_STRING_MAGIC.
 *  @field{char[]}[str] The string contents.
 */
typedef struct {
  size_t refcount;
  char str[];
} FbldString;

typedef struct {
  const char* file;
  size_t line;
  size_t column;
} FbldLoc;

typedef struct {
  FbldLoc loc;
  FbldString* str;
} FbldText;

typedef struct {
  size_t size;
  FbldText** xs;
} FbldTextV;

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
 *   @i Prints an error to stderr and aborst the program in case of error.
 */
void FbldPrintMarkup(FbldMarkup* markup);

#endif // _FBLD_H_
