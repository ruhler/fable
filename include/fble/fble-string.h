// fble-string.h --
//   Header file for FbleString type.

#ifndef FBLE_STRING_H_
#define FBLE_STRING_H_

#include <sys/types.h>    // for size_t

#define FBLE_STRING_MAGIC 0x516179

/**
 * A reference counted string of characters.
 *
 * Pass by pointer. Explicit copy and free required.
 *
 * Note: The magic field is set to FBLE_STRING_MAGIC and is used to detect
 * double frees of FbleString, which we have had trouble with in the past.
 */
typedef struct {
  size_t refcount;
  size_t magic;
  char str[];
} FbleString;

/** A vector of FbleString. */
typedef struct {
  size_t size;
  FbleString** xs;
} FbleStringV;

// FbleNewString --
//   Allocate a new FbleString.
//
// Inputs:
//   str - the contents of the string.
//
// Results:
//   A newly allocated string with a reference count that should be released
//   using FbleFreeString when no longer needed.
//   Does not take ownership of str - makes a copy instead.
FbleString* FbleNewString(const char* str);

// FbleCopyString -- 
//   Make a (possibly shared) copy of the given string.
//
// Inputs:
//   string - the string to copy.
// 
// Results:
//   The new (possibly shared) copy of the string.
//
// Side effects:
//   The user should arrange for FbleFreeString to be called on this string
//   copy when it is no longer needed.
FbleString* FbleCopyString(FbleString* string);

// FbleFreeString -- 
//   Free resources associated with the given string.
//
// Inputs:
//   string - the string to free.
//
// Side effects:
//   Frees resources associated the string and its contents.
void FbleFreeString(FbleString* string);

#endif // FBLE_STRING_H_
