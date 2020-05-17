// fble-name.h --
//   Header file for fble name and associated types.

#ifndef FBLE_NAME_H_
#define FBLE_NAME_H_

#include <sys/types.h>    // for size_t

#include "fble-alloc.h"

// FbleString --
//   A reference counted string.
//
// Note: The checksum field is used internally to detect double frees of
// FbleString, which we have had trouble with in the past.
typedef struct {
  size_t refcount;
  const char* str;
  size_t checksum;
} FbleString;

// FbleNewString --
//   Allocate a new FbleString.
//
// Inputs:
//   arena - arena to use for allocations.
//   str - the contents of the string.
//
// Results:
//   A newly allocated string with a reference count that should be released
//   using FbleReleaseString when no longer needed.
//   Does not take ownership of str - makes a copy instead.
FbleString* FbleNewString(FbleArena* arena, const char* str);

// FbleRetainString -- 
//   Increment the refcount on a string.
//
// Inputs:
//   string - the string to increment the reference count on.
// 
// Results:
//   The input string, for convenience.
//
// Side effects:
//   Increments the reference count on the string. The user should arrange for
//   FbleReleaseString to be called on the string when this reference of it is
//   no longer needed.
FbleString* FbleRetainString(FbleString* string);

// FbleReleaseString -- 
//   Decrement the refcount on a string, freeing resources associated with the
//   string if the refcount goes to zero.
//
// Inputs:
//   arena - arena to use for allocations.
//   string - the string to decrement the reference count on.
//
// Side effects:
//   Decrements the reference count of the string, freeing it and its str
//   contents if the reference count goes to zero.
void FbleReleaseString(FbleArena* arena, FbleString* string);

// FbleLoc --
//   Represents a location in a source file.
//
// Fields:
//   source - The name of the source file or other description of the source
//            of the program text.
//   line - The line within the file for the location.
//   col - The column within the line for the location.
typedef struct {
  FbleString* source;
  int line;
  int col;
} FbleLoc;

// FbleCopyLoc --
//   Make a copy of a location. In particular, increments the reference count
//   on the filename.
//
// Inputs:
//   loc - the loc to copy.
//
// Result:
//   A copy of the loc.
//
// Side effects:
//   Increments the reference count on the filename used in the loc.
FbleLoc FbleCopyLoc(FbleLoc loc);

// FbleFreeLoc --
//   Free resources associated with the given loc.
//
// Inputs:
//   arena - arena to use for allocations
//   loc - the location to free resources of.
//
// Side effects
//   Decrements the refcount on the loc's source filename.
void FbleFreeLoc(FbleArena* arena, FbleLoc loc);

// FbleNamespace --
//   Enum used to distinguish among different name spaces.
typedef enum {
  FBLE_NORMAL_NAME_SPACE,
  FBLE_TYPE_NAME_SPACE,
  FBLE_MODULE_NAME_SPACE,
} FbleNameSpace;

// FbleName -- 
//   A name along with its associated location in a source file. The location
//   is typically used for error reporting purposes.
typedef struct {
  const char* name;
  FbleNameSpace space;
  FbleLoc loc;
} FbleName;

// FbleFreeName --
//   Free resources associated with a name.
//
// Inputs:
//   arena - arena to use for allocations.
//   name - the name to free resources of.
//
// Side effects:
//   Frees resources associated with the name.
void FbleFreeName(FbleArena* arena, FbleName name);

#endif // FBLE_NAME_H_
