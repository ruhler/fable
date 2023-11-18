/**
 * @file fble-load.h
 *  Load fble source files.
 */

#ifndef FBLE_LOAD_H_
#define FBLE_LOAD_H_

#include <stdio.h>  // for FILE

#include "fble-module-path.h"
#include "fble-string.h"

/**
 * Abstract syntax tree of an expression.
 */
typedef struct FbleExpr FbleExpr;

/**
 * Abstract syntax tree of a module.
 *
 * Either one or both of 'type' and 'value' fields may be supplied. The
 * 'value' field is required to run or generate code for the module. The type
 * of the module can be determined either from the 'type' field or the type of
 * the 'value' field. If both 'type' and 'value' are supplied, the typechecker
 * will check that they describe the same type for the module.
 */
typedef struct {
  /** the path to the module. */
  FbleModulePath* path;

  /** list of modules this module depends on. */
  FbleModulePathV deps;

  /** abstract syntax of the module type. May be NULL. */
  FbleExpr* type;

  /** abstract syntax of the module implementation. May be NULL. */
  FbleExpr* value;
} FbleLoadedModule;

/** Vector of FbleLoadedModule. */
typedef struct {
  size_t size;            /**< Number of elements. */
  FbleLoadedModule* xs;   /**< Elements. */
} FbleLoadedModuleV;

/**
 * Abstract syntax tree for a full fble program.
 *
 * The program is represented as a list of dependant module in topological
 * dependancy order. Later modules in the list may depend on earlier modules
 * in the list, but not the other way around.
 *
 * The last module in the list is the main program. The module path for the
 * main module is the empty path /%.
 */
typedef struct {
  FbleLoadedModuleV modules;  /**< Program modules. */
} FbleLoadedProgram;

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
 * @func[FbleSearchPathAppend] Appends a module root directory to a search path.
 *  @arg[FbleSearchPath*][path    ] The path to append to.
 *  @arg[const char*    ][root_dir] The directory to add to the path. Borrowed.
 *
 *  @sideeffects
 *   Adds root_dir to the search path.
 */
void FbleSearchPathAppend(FbleSearchPath* path, const char* root_dir);

/**
 * @func[FbleSearchPathAppendString] Appends a module root directory to a search path.
 *  @arg[FbleSearchPath*][path    ] The path to append to.
 *  @arg[FbleString*    ][root_dir] The directory to add to the path. Borrowed.
 *
 *  @sideeffects
 *   Adds root_dir to the search path.
 */
void FbleSearchPathAppendString(FbleSearchPath* path, FbleString* root_dir);

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
 *  @returns FbleLoadedProgram*
 *   The parsed program, or NULL in case of error.
 *
 *  @sideeffects
 *   @i Prints an error message to stderr if the program cannot be parsed.
 *   @item
 *    The user should call FbleFreeLoadedProgram to free resources
 *    associated with the given program when it is no longer needed.
 *   @item
 *    The user should free strings added to build_deps when no longer
 *    needed, including in the case when program loading fails.
 */
FbleLoadedProgram* FbleLoad(FbleSearchPath* search_path, FbleModulePath* module_path, FbleStringV* build_deps);

/**
 * @func[FbleFreeLoadedProgram] Frees an FbleLoadedProgram.
 *  @arg[FbleLoadedProgram*][program] The program to free. May be NULL.
 *
 *  @sideeffects
 *   Frees resources associated with the given program.
 */
void FbleFreeLoadedProgram(FbleLoadedProgram* program);

/**
 * @func[FbleSaveBuildDeps] Saves a depfile.
 *  Save the list of build dependencies to a depfile suitable for use with
 *  ninja or make.
 *
 *  For example, it would generate something like:
 *
 *  @code[txt]
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
