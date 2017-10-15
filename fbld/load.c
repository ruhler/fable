// load.c --
//   This file implements routines for loading fbld module declarations and
//   definitions.

#include <assert.h>   // for assert
#include <stdio.h>    // for sprintf, fprintf, stderr
#include <string.h>   // for strlen
#include <unistd.h>   // for access

#include "fblc.h"
#include "fbld.h"

static char* FindModuleFile(FblcArena* arena, FbldStringV* path, const char* name);


// FindModuleFile --
//   Find the name of a module declaration or definition file on disk.
//
// Inputs:
//   arena - Arena to use for allocating the returned filename.
//   path - The module search path.
//   name - The name of the module.
//
// Result:
//   The filename of the requested module file or NULL if no such file could
//   be found.
//
// Side effects:
//   Tests whether files exist on the search path.
//
// Allocations:
//   The user is responsible for freeing the returned filename if not null
//   using the arena passed to this function.
static char* FindModuleFile(FblcArena* arena, FbldStringV* path, const char* name)
{
  for (size_t i = 0; i < path->size; ++i) {
    size_t length = strlen(path->xs[i]) + 1 + strlen(name) + strlen(".fbld") + 1;
    char* filename = arena->alloc(arena, sizeof(char) * length);
    sprintf(filename, "%s/%s%s", path->xs[i], name, ".fbld");
    if (access(filename, F_OK) == 0) {
      return filename;
    }
    arena->free(arena, filename);
  }
  return NULL;
}

// FbldLoadInterf -- see documentation in fbld.h
FbldInterf* FbldLoadInterf(FblcArena* arena, FbldStringV* path, const char* name, FbldProgram* prgm)
{
  // Return the existing interface declaration if it has already been loaded.
  for (size_t i = 0; i < prgm->interfv.size; ++i) {
    if (FbldNamesEqual(name, prgm->interfv.xs[i]->name->name)) {
      return prgm->interfv.xs[i];
    }
  }

  char* filename = FindModuleFile(arena, path, name);
  if (filename == NULL) {
    fprintf(stderr, "unable to locate %s.fbld on search path\n", name);
    return NULL;
  }

  // Parse the module.
  FbldInterf* interf = FbldParseInterf(arena, filename);
  if (interf == NULL) {
    fprintf(stderr, "failed to parse module from %s\n", filename);
    return NULL;
  }

  if (!FbldNamesEqual(interf->name->name, name)) {
    FbldReportError("Expected '%s', but found '%s'\n", interf->name->loc, name, interf->name->name);
    return NULL;
  }
  FblcVectorAppend(arena, prgm->interfv, interf);

  // Check that this declaration is valid.
  // TODO: detect and abort if the module module recursively depends on itself.
  if (!FbldCheckInterf(arena, path, interf, prgm)) {
    return NULL;
  }

  return interf;
}

// FbldLoadModuleHeader -- see documentation in fbld.h
FbldModule* FbldLoadModuleHeader(FblcArena* arena, FbldStringV* path, const char* name, FbldProgram* prgm)
{
  // Return the existing declaration if it has already been loaded.
  for (size_t i = 0; i < prgm->mheaderv.size; ++i) {
    if (FbldNamesEqual(name, prgm->mheaderv.xs[i]->name->name)) {
      return prgm->mheaderv.xs[i];
    }
  }

  // Parse the module declaration if we haven't already.
  char* filename = FindModuleFile(arena, path, name);
  if (filename == NULL) {
    fprintf(stderr, "unable to locate %s.fbld on search path\n", name);
    return NULL;
  }

  FbldModule* mdecl = FbldParseModule(arena, filename);
  if (mdecl == NULL) {
    fprintf(stderr, "failed to parse module from %s\n", filename);
    return NULL;
  }

  if (!FbldNamesEqual(mdecl->name->name, name)) {
    FbldReportError("Expected '%s', but found '%s'\n", mdecl->name->loc, name, mdecl->name->name);
    return NULL;
  }

  FblcVectorAppend(arena, prgm->mheaderv, mdecl);

  // Check that this definition is valid.
  // TODO: detect and abort if the mdecl recursively depends on itself.
  if (!FbldCheckModuleHeader(arena, path, mdecl, prgm)) {
    return NULL;
  }
  return mdecl;
}

// FbldLoadModule -- see documentation in fbld.h
FbldModule* FbldLoadModule(FblcArena* arena, FbldStringV* path, const char* name, FbldProgram* prgm)
{
  // Return the existing module declaration if it has already been loaded.
  for (size_t i = 0; i < prgm->modulev.size; ++i) {
    if (FbldNamesEqual(name, prgm->modulev.xs[i]->name->name)) {
      return prgm->modulev.xs[i];
    }
  }

  FbldModule* module = FbldLoadModuleHeader(arena, path, name, prgm);
  if  (module == NULL) {
    return NULL;
  }

  assert(FbldNamesEqual(module->name->name, name));
  FblcVectorAppend(arena, prgm->modulev, module);

  // Check that this definition is valid.
  // TODO: detect and abort if the module recursively depends on itself.
  if (!FbldCheckModule(arena, path, module, prgm)) {
    return NULL;
  }

  return module;
}

// FbldLoadModules -- see documentation in fbld.h
bool FbldLoadModules(FblcArena* arena, FbldStringV* path, const char* name, FbldProgram* prgm)
{
  if (!FbldLoadModule(arena, path, name, prgm)) {
    return false;
  }

  // Load all the modules still left to load.
  // Note: More module headers will be added as modules are loaded, which
  // means that prgm->mheaderv->size will often increase during the body of
  // the loop. Because elements are only ever appended to mheaderv, this
  // should be okay. TODO: detect and abort if the module recursively depends
  // on itself.
  for (size_t i = 0; i < prgm->mheaderv.size; ++i) {
    if (!FbldLoadModule(arena, path, prgm->mheaderv.xs[i]->name->name, prgm)) {
      return false;
    }
  }
  return true;
}
