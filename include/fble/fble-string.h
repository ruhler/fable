/**
 * @file fble-string.h
 * FbleString API.
 */

#ifndef FBLE_STRING_H_
#define FBLE_STRING_H_

#include <sys/types.h>    // for size_t

/**
 * Magic number used in FbleString.
 */
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
  size_t refcount;    /**< The reference count. */
  size_t magic;       /**< FBLE_STRING_MAGIC. */
  char str[];         /**< The string contents. */
} FbleString;

/** Vector of FbleString. */
typedef struct {
  size_t size;      /**< Number of elements. */
  FbleString** xs;  /**< Elements. */
} FbleStringV;

/**
 * Allocates an FbleString.
 *
 * @param str  The contents of the string. Borrowed. This function does not
 *             take ownership of str, it makes a copy internally instead.
 *
 * @returns
 *   A newly allocated string with a reference count that should be released
 *   using FbleFreeString when no longer needed.
 */
FbleString* FbleNewString(const char* str);

/**
 * Copies an FbleString.
 *
 * @param string  The string to copy.
 * 
 * @returns
 *   The new (possibly shared) copy of the string.
 *
 * @sideeffects
 *   The user should arrange for FbleFreeString to be called on this string
 *   copy when it is no longer needed.
 */
FbleString* FbleCopyString(FbleString* string);

/**
 * Frees an FbleString.
 *
 * @param string  The string to free.
 *
 * @sideeffects
 *   Frees resources associated the string and its contents.
 */
void FbleFreeString(FbleString* string);

#endif // FBLE_STRING_H_
