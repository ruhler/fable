/**
 * @file fble-string.h
 *  FbleString API.
 */

#ifndef FBLE_STRING_H_
#define FBLE_STRING_H_

#include <sys/types.h>    // for size_t

/**
 * Magic number used in FbleString.
 */
typedef enum {
  FBLE_STRING_MAGIC = 0x516179
} FbleStringMagic;

/**
 * @struct[FbleString] A reference counted string of characters.
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
  FbleStringMagic magic;
  char str[];
} FbleString;

/**
 * @struct[FbleStringV] Vector of FbleString.
 *  @field[size_t][size] Number of elements.
 *  @field[FbleString**][xs] Elements.
 */
typedef struct {
  size_t size;
  FbleString** xs;
} FbleStringV;

/**
 * @func[FbleNewString] Allocates an FbleString.
 *  @arg[const char*] str
 *   The contents of the string. Borrowed. This function does not take
 *   ownership of str, it makes a copy internally instead.
 *
 *  @returns FbleString*
 *   A newly allocated string with a reference count that should be released
 *   using FbleFreeString when no longer needed.
 */
FbleString* FbleNewString(const char* str);

/**
 * @func[FbleCopyString] Copies an FbleString.
 *  @arg[FbleString*][string] The string to copy.
 * 
 *  @returns FbleString*
 *   The new (possibly shared) copy of the string.
 *
 *  @sideeffects
 *   The user should arrange for FbleFreeString to be called on this string
 *   copy when it is no longer needed.
 */
FbleString* FbleCopyString(FbleString* string);

/**
 * @func[FbleFreeString] Frees an FbleString.
 *  @arg[FbleString*][string] The string to free.
 *
 *  @sideeffects
 *   Frees resources associated the string and its contents.
 */
void FbleFreeString(FbleString* string);

#endif // FBLE_STRING_H_
