/**
 * @file fble-load.h
 *  Load fble source files.
 */

#ifndef FBLE_LOAD_H_
#define FBLE_LOAD_H_

#include <stdio.h>  // for FILE

#include "fble-module-path.h"
#include "fble-program.h"
#include "fble-string.h"

/**
 * @func[FbleParse] Parses an fble module from a file.
 *  @arg[FbleString*] filename
 *   The name of the file to parse the program from.
 *  @arg[FbleModulePathV*] deps
 *   A list of the modules that the parsed expression references. Modules will
 *   appear at most once in the list.
 *
 *  @returns FbleExpr*
 *   The parsed program, or NULL in case of error.
 *
 *  @sideeffects
 *   @i Prints an error message to stderr if the program cannot be parsed.
 *   @item
 *    Appends module paths in the parsed expression to deps, which is assumed
 *    to be a pre-initialized vector. The caller is responsible for calling
 *    FbleFreeModulePath on each path when it is no longer needed.
 */
FbleExpr* FbleParse(FbleString* filename, FbleModulePathV* deps);

/**
 * Module search path.
 *
 * A list of directories to use as the root of an fble file hierarchy for
 * locating .fble files corresponding to a module path.
 *
 * The directories are searched in order for the first matching module.
 */
typedef struct FbleSearchPath FbleSearchPath;

/**
 * @func[FbleNewSearchPath] Allocates a new, empty search path.
 *  @returns FbleSearchPath*
 *   A new empty search path.
 *
 *  @sideeffects
 *   The search path should be freed with FbleFreeSearchPath when it is no
 *   longer needed.
 */
FbleSearchPath* FbleNewSearchPath();

/**
 * @func[FbleFreeSearchPath] Frees resources associated with an FbleSearchPath.
 *  @arg[FbleSearchPath*][path] The path to free.
 *
 *  @sideeffects
 *   @i Frees resources associated with the search path.
 */
void FbleFreeSearchPath(FbleSearchPath* path);

/**
 * @func[FbleAppendToSearchPath] Appends a module root directory to a search path.
 *  @arg[FbleSearchPath*][path    ] The path to append to.
 *  @arg[const char*    ][root_dir] The directory to add to the path. Borrowed.
 *
 *  @sideeffects
 *   Adds root_dir to the search path.
 */
void FbleAppendToSearchPath(FbleSearchPath* path, const char* root_dir);

/**
 * @func[FbleAppendStringToSearchPath] Appends a module root directory to a search path.
 *  @arg[FbleSearchPath*][path    ] The path to append to.
 *  @arg[FbleString*    ][root_dir] The directory to add to the path. Borrowed.
 *
 *  @sideeffects
 *   Adds root_dir to the search path.
 */
void FbleAppendStringToSearchPath(FbleSearchPath* path, FbleString* root_dir);

/**
 * @func[FbleFindPackage] Finds a package in the package path.
 *  Searches for the package in the directories listed in the colon-separated
 *  FBLE_PACKAGE_PATH environment variable and in the standard package path
 *  directories.
 *
 *  @arg[const char*][package] The name of the package.
 *
 *  @returns FbleString*
 *   The module root directory for the package, or NULL in case no such
 *   package is found.
 *
 *  @sideeffects
 *   Allocates an FbleString that should be freed with FbleFreeString when no
 *   longer needed.
 */
FbleString* FbleFindPackage(const char* package);

/**
 * @func[FbleLoad] Loads an fble program.
 *  @arg[FbleSearchPath*] search_path
 *   The search path to use for location .fble files. Borrowed.
 *  @arg[FbleModulePath*] module_path
 *   The module path for the main module to load. Borrowed.
 *  @arg[FbleStringV*] build_deps
 *   Output to store list of files the load depended on. This should be a
 *   preinitialized vector, or NULL.
 *
 *  @returns FbleProgram*
 *   The parsed program, or NULL in case of error.
 *
 *  @sideeffects
 *   @i Prints an error message to stderr if the program cannot be parsed.
 *   @item
 *    The user should call FbleFreeProgram to free resources
 *    associated with the given program when it is no longer needed.
 *   @item
 *    The user should free strings added to build_deps when no longer
 *    needed, including in the case when program loading fails.
 */
FbleProgram* FbleLoad(FbleSearchPath* search_path, FbleModulePath* module_path, FbleStringV* build_deps);

/**
 * @func[FbleSaveBuildDeps] Saves a depfile.
 *  Save the list of build dependencies to a depfile suitable for use with
 *  ninja or make.
 *
 *  For example, it would generate something like:
 *
 *  @code[txt] @
 *   target: build_deps1 build_deps2 build_deps3
 *     build_deps4 build_deps5 ...
 *
 *  @arg[FILE*] fout
 *   Output stream to write the dependency file to.
 *  @arg[const char*] target
 *   The target of the dependencies to generate.
 *  @arg[FbleStringV] build_deps
 *   The list of file dependencies.
 *
 *  @sideeffects
 *   @i Creates a build dependency file.
 *   @i Outputs an error message to stderr in case of error.
 */
void FbleSaveBuildDeps(FILE* fout, const char* target, FbleStringV build_deps);

#endif // FBLE_LOAD_H_
