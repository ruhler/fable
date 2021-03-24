// fble-name.h --
//   Header file for fble name and associated types.

#ifndef FBLE_NAME_H_
#define FBLE_NAME_H_

#include <sys/types.h>    // for size_t

#include "fble-alloc.h"

// FbleString --
//   A string of characters.
//
// Pass by pointer. Explicit copy and free required.
//
// Note: The magic field is set to FBLE_STRING_MAGIC and is used to detect
// double frees of FbleString, which we have had trouble with in the past.
#define FBLE_STRING_MAGIC 0x516179
typedef struct {
  size_t refcount;
  size_t magic;
  char str[];
} FbleString;

// FbleStringV --
//   A vector of FbleString. 
typedef struct {
  size_t size;
  FbleString** xs;
} FbleStringV;

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
//   arena - arena to use for allocations.
//   string - the string to free.
//
// Side effects:
//   Frees resources associated the string and its contents.
void FbleFreeString(FbleArena* arena, FbleString* string);

// FbleLoc --
//   Represents a location in a source file.
//
// Pass by value. Explicit copy and free required.
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
//   Make a (possibly shared) copy of a location.
//
// Inputs:
//   loc - the loc to copy.
//
// Result:
//   A (possibly shared) copy of the loc.
//
// Side effects:
//   The user should call FbleFreeLoc on the returned loc when it is no longer
//   needed.
FbleLoc FbleCopyLoc(FbleLoc loc);

// FbleFreeLoc --
//   Free resources associated with the given loc.
//
// Inputs:
//   arena - arena to use for allocations
//   loc - the location to free resources of.
//
// Side effects
//   Frees resources associated with the given loc.
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
// Pass by value. Explicit copy and free required.
//
// The name and loc fields are owned by this FbleName.
typedef struct {
  FbleString* name;
  FbleNameSpace space;
  FbleLoc loc;
} FbleName;

// FbleNameV --
//   A vector of FbleNames.
typedef struct {
  size_t size;
  FbleName* xs;
} FbleNameV;

// FbleCopyName --
//   Make a (possibly shared) copy of the name.
//
// Inputs:
//   arena - the arena to use for allocations.
//   name - the name to copy.
//
// Results:
//   A (possibly shared) copy of the name.
//
// Side effects:
//   The user should call FbleFreeName on the returned name when it is no
//   longer needed.
FbleName FbleCopyName(FbleArena* arena, FbleName name);

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
