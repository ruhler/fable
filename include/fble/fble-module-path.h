/**
 * @file fble-module-path.h
 * FbleModulePath API.
 */

#ifndef FBLE_MODULE_PATH_H_
#define FBLE_MODULE_PATH_H_

#include "fble-loc.h"    // for FbleLoc
#include "fble-name.h"    // for FbleName

/**
 * Magic number used in FbleModulePath.
 */
#define FBLE_MODULE_PATH_MAGIC 0x77806584

/**
 * A module path.
 *
 * For example: /Foo/Bar%.
 *
 * Pass by pointer. Explicitly copy and free required.
 * 
 * By convention, all names in the path belong to the FBLE_NORMAL_NAME_SPACE.
 *  
 * The magic field is set to FBLE_MODULE_PATH_MAGIC and is used to detect
 * double frees of FbleModulePath.
 */
typedef struct {
  size_t refcount;    /**< Current reference count. */
  size_t magic;       /**< FBLE_MODULE_PATH_MAGIC. */
  FbleLoc loc;        /**< Location associated with the path. */
  FbleNameV path;     /**< Name of the path. */
} FbleModulePath;

/** A vector of FbleModulePath. */
typedef struct {
  size_t size;          /**< Number of elements. */
  FbleModulePath** xs;  /**< Elements. */
} FbleModulePathV;

/**
 * Allocates a new FbleModulePath.
 *
 * @param loc  The location of the module path. Borrowed.
 *
 * @returns
 *   A newly allocated empty module path.
 *
 * @sideeffects
 *   Allocates a new module path that the user should free with
 *   FbleFreeModulePath when no longer needed.
 */
FbleModulePath* FbleNewModulePath(FbleLoc loc);

/**
 * Creates an FbleName for a module path.
 *
 * @param path  The path to construct an FbleName for.
 *
 * @returns
 *   An FbleName describing the full module path. For example: "/Foo/Bar%".
 *
 * @sideeffects
 *   The caller should call FbleFreeName on the returned name when no longer
 *   needed.
 */
FbleName FbleModulePathName(FbleModulePath* path);

/**
 * Prints a module path.
 *
 * @param stream  The stream to print to
 * @param path    The path to print
 *
 * @sideeffects
 *   Outputs the path to the given file.
 */
void FblePrintModulePath(FILE* stream, FbleModulePath* path);

/**
 * Tests if two module paths are equal.
 *
 * Two paths are considered equal if they have the same sequence of module
 * names. Locations are not relevant for this check.
 *
 * @param a   The first path.
 * @param b   The second path.
 *
 * @returns
 *   true if the first path equals the second, false otherwise.
 *
 * @sideeffects
 *   None.
 */
bool FbleModulePathsEqual(FbleModulePath* a, FbleModulePath* b);

/**
 * Checks if a module belongs to a package.
 *
 * @param module    The module path.
 * @param package   The module path describing the package.
 *
 * @returns
 *   True if the module belongs to the package, false otherwise.
 *
 * @sideeffects
 *   None.
 */
bool FbleModuleBelongsToPackage(FbleModulePath* module, FbleModulePath* package);

/**
 * Parses an FbleModulePath.
 *
 * @param string   The string to parse the path from.
 *
 * @returns
 *   The parsed path, or NULL in case of error.
 *
 * @sideeffects
 * * Prints an error message to stderr if the path cannot be parsed.
 */
FbleModulePath* FbleParseModulePath(const char* string);

/**
 * Copies an FbleModulePath.
 *
 * @param path  The path to copy.
 * 
 * @returns
 *   The new (possibly shared) copy of the path.
 *
 * @sideeffects
 *   The user should arrange for FbleFreeModulePath to be called on this path
 *   copy when it is no longer needed.
 */
FbleModulePath* FbleCopyModulePath(FbleModulePath* path);

/**
 * Frees an FbleModulePath.
 *
 * @param path  The path to free.
 *
 * @sideeffects
 *   Frees resources associated with the path and its contents.
 */
void FbleFreeModulePath(FbleModulePath* path);

#endif // FBLE_MODULE_PATH_H_
