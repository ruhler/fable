// fble-name.h --
//   Header file for fble name and associated types.

#ifndef FBLE_NAME_H_
#define FBLE_NAME_H_

#include <sys/types.h>    // for size_t

#include "fble-alloc.h"

// FbleString --
//   A reference counted string.
//
// Note: The magic field is set to FBLE_STRING_MAGIC and is used to detect
// double frees of FbleString, which we have had trouble with in the past.
typedef struct {
  size_t refcount;
  size_t magic;
  const char* str;
} FbleString;

// FBLE_STRING_MAGIC --
//   The magic number we store in every FbleString, to help catch double free.
#define FBLE_STRING_MAGIC 0x516179

// FbleNewString --
//   Allocate a new FbleString.
//
// Inputs:
//   arena - arena to use for allocations.
//   str - the contents of the string.
//
// Results:
//   A newly allocated string with a reference count that should be released
//   using FbleFreeString when no longer needed.
//   Does not take ownership of str - makes a copy instead.
FbleString* FbleNewString(FbleArena* arena, const char* str);

// FbleCopyString -- 
//   Make a copy of the given string.
//
// Inputs:
//   string - the string to copy.
// 
// Results:
//   The new copy of the string, which may be the same pointer as the given
//   string.
//
// Side effects:
//   The user should arrange for FbleFreeString to be called on this string
//   copy when it is no longer needed.
FbleString* FbleCopyString(FbleString* string);

// FbleFreeString -- 
//   Free resources associated with the given string.
//
// Inputs:
//   arena - arena to use for allocations.
//   string - the string to free.
//
// Side effects:
//   Frees resources associated the string and its contents.
void FbleFreeString(FbleArena* arena, FbleString* string);

// FbleLoc --
//   Represents a location in a source file.
//
// Fields:
//   source - The name of the source file or other description of the source
//            of the program text. Owned by this FbleLoc.
//   line - The line within the file for the location.
//   col - The column within the line for the location.
typedef struct {
  FbleString* source;
  int line;
  int col;
} FbleLoc;

// FbleCopyLoc --
//   Make a copy of a location. In particular, increments the reference count
//   on the source filename.
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
//
// name and loc are owned by this FbleName.
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
