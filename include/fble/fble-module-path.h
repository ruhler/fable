/**
 * @file fble-module-path.h
 *  FbleModulePath API.
 */

#ifndef FBLE_MODULE_PATH_H_
#define FBLE_MODULE_PATH_H_

#include "fble-loc.h"    // for FbleLoc
#include "fble-name.h"    // for FbleName

/**
 * @enum[FbleModulePathMagic] Magic number used in FbleModulePath.
 *  @field[FBLE_MODULE_PATH_MAGIC] The magic path value.
 */
typedef enum {
  FBLE_MODULE_PATH_MAGIC = 0x77806584
} FbleModulePathMagic;

/**
 * @struct[FbleModulePath] A module path.
 *  For example: @l{/Foo/Bar%}.
 *
 *  Pass by pointer. Explicitly copy and free required.
 * 
 *  By convention, all names in the path belong to the FBLE_NORMAL_NAME_SPACE.
 *  
 *  The magic field is set to FBLE_MODULE_PATH_MAGIC and is used to detect
 *  double frees of FbleModulePath.
 *
 *  @field[size_t][refcount] Current reference count.
 *  @field[FbleModulePathMagic][magic] FBLE_MODULE_PATH_MAGIC.
 *  @field[FbleLoc][loc] Location associated with the path.
 *  @field[FbleNameV][path] Name of the path.
 */
typedef struct {
  size_t refcount;
  FbleModulePathMagic magic;
  FbleLoc loc;
  FbleNameV path;
} FbleModulePath;

/**
 * @struct[FbleModulePathV] A vector of FbleModulePath.
 *  @field[size_t][size] Number of elements.
 *  @field[FbleModulePath**][xs] Elements.
 */
typedef struct {
  size_t size;
  FbleModulePath** xs;
} FbleModulePathV;

/**
 * @func[FbleNewModulePath] Allocates a new FbleModulePath.
 *  @arg[FbleLoc][loc] The location of the module path. Borrowed.
 *
 *  @returns FbleModulePath*
 *   A newly allocated empty module path.
 *
 *  @sideeffects
 *   Allocates a new module path that the user should free with
 *   FbleFreeModulePath when no longer needed.
 */
FbleModulePath* FbleNewModulePath(FbleLoc loc);

/**
 * @func[FbleModulePathName] Creates an FbleName for a module path.
 *  @arg[FbleModulePath*][path] The path to construct an FbleName for.
 *
 *  @returns FbleName
 *   An FbleName describing the full module path. For example: @l{/Foo/Bar%}.
 *
 *  @sideeffects
 *   The caller should call FbleFreeName on the returned name when no longer
 *   needed.
 */
FbleName FbleModulePathName(FbleModulePath* path);

/**
 * @func[FblePrintModulePath] Prints a module path.
 *  @arg[FILE*          ][stream] The stream to print to
 *  @arg[FbleModulePath*][path  ] The path to print
 *
 *  @sideeffects
 *   Outputs the path to the given file.
 */
void FblePrintModulePath(FILE* stream, FbleModulePath* path);

/**
 * @func[FbleModulePathsEqual] Tests if two module paths are equal.
 *  Two paths are considered equal if they have the same sequence of module
 *  names. Locations are not relevant for this check.
 *
 *  @arg[FbleModulePath*][a] The first path.
 *  @arg[FbleModulePath*][b] The second path.
 *
 *  @returns bool
 *   true if the first path equals the second, false otherwise.
 *
 *  @sideeffects
 *   None.
 */
bool FbleModulePathsEqual(FbleModulePath* a, FbleModulePath* b);

/**
 * @func[FbleModuleBelongsToPackage] Checks if a module belongs to a package.
 *  @arg[FbleModulePath*][module ] The module path.
 *  @arg[FbleModulePath*][package] The module path describing the package.
 *  
 *  @returns bool
 *   True if the module belongs to the package, false otherwise.
 *  
 *  @sideeffects
 *   None.
 */
bool FbleModuleBelongsToPackage(FbleModulePath* module, FbleModulePath* package);

/**
 * @func[FbleParseModulePath] Parses an FbleModulePath.
 *  @arg[const char*][string] The string to parse the path from.
 *
 *  @returns FbleModulePath*
 *   The parsed path, or NULL in case of error.
 *
 *  @sideeffects
 *   Prints an error message to stderr if the path cannot be parsed.
 */
FbleModulePath* FbleParseModulePath(const char* string);

/**
 * @func[FbleCopyModulePath] Copies an FbleModulePath.
 *  @arg[FbleModulePath*][path] The path to copy.
 * 
 *  @returns FbleModulePath*
 *   The new (possibly shared) copy of the path.
 *
 *  @sideeffects
 *   The user should arrange for FbleFreeModulePath to be called on this path
 *   copy when it is no longer needed.
 */
FbleModulePath* FbleCopyModulePath(FbleModulePath* path);

/**
 * @func[FbleFreeModulePath] Frees an FbleModulePath.
 *  @arg[FbleModulePath*][path] The path to free.
 *
 *  @sideeffects
 *   Frees resources associated with the path and its contents.
 */
void FbleFreeModulePath(FbleModulePath* path);

#endif // FBLE_MODULE_PATH_H_
