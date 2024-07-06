/**
 * @file fbld.h
 *  General header file for fbld types and functions.
 */

#ifndef _FBLD_H_
#define _FBLD_H_

#include <stdbool.h>    // for bool
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
  FbldString* file;
  size_t line;
  size_t column;
} FbldLoc;

typedef struct {
  FbldLoc loc;
  FbldString* str;
} FbldText;

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
  FbldText* text;         // Plain text or command name.
  FbldMarkupV markups;    // Sequence of markups or command args.
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
 * @func[FbldParse] Parses a sequence of fbld files.
 *  @arg[size_t][argc] The number of input files.
 *  @arg[const char**][argv] The names of input files.
 *
 *  @returns[FbldMarkup*] The parsed markup, or NULL in case of failure.
 *
 *  @sideeffects
 *   @i Prints an error to stderr in case of error.
 *   @item
 *    The user is responsible for calling FbldFreeMarkup on the returned
 *    markup object when no longer needed.
 */
FbldMarkup* FbldParse(size_t argc, const char** argv);

/**
 * @func[FbldEval] Evaluates an fbld document.
 *  @arg[FbldMarkup*][markup] The markup to evaluate.
 *
 *  @returns[FbldMarkup*] The evaluated markup, or NULL in case of failure.
 *
 *  @sideeffects
 *   @i Prints an error to stderr in case of error.
 *   @item
 *    The user is responsible for calling FbldFreeMarkup on the returned
 *    markup object when no longer needed.
 */
FbldMarkup* FbldEval(FbldMarkup* markup);

/**
 * @func[FbldPrintMarkup] Prints markup to stdout.
 *  @arg[FbldMarkup*][markup] The markup to print.
 *
 *  @returns[bool] True on success, false in case of error.
 *
 *  @sideeffects
 *   @i Prints an error to stderr in case of error.
 */
bool FbldPrintMarkup(FbldMarkup* markup);

#endif // _FBLD_H_
