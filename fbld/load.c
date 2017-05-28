// load.c --
//   This file implements routines for loading fbld module declarations and
//   definitions.

#include <assert.h>   // for assert
#include <stdio.h>    // for sprintf, fprintf, stderr
#include <string.h>   // for strcmp, strlen
#include <unistd.h>   // for access

#include "fblc.h"
#include "fbld.h"

static char* FindModuleFile(FblcArena* arena, FbldStringV* path, FbldName name, const char* extension);


// FindModuleFile --
//   Find the name of a module declaration or definition file on disk.
//
// Inputs:
//   arena - Arena to use for allocating the returned filename.
//   path - The module search path.
//   name - The name of the module.
//   extension - The extension of the module file: ".mdecl" or ".mdefn".
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
static char* FindModuleFile(FblcArena* arena, FbldStringV* path, FbldName name, const char* extension)
{
  for (size_t i = 0; i < path->size; ++i) {
    size_t length = strlen(path->xs[i]) + 1 + strlen(name) + strlen(extension) + 1;
    char* filename = arena->alloc(arena, sizeof(char) * length);
    sprintf(filename, "%s/%s%s", path->xs[i], name, extension);
    if (access(filename, F_OK) == 0) {
      return filename;
    }
    arena->free(arena, filename);
  }
  return NULL;
}

// FbldLoadMDecl -- see documentation in fbld.h
FbldMDecl* FbldLoadMDecl(FblcArena* arena, FbldStringV* path, FbldName name, FbldMDeclV* mdeclv)
{
  // Return the existing module declaration if it has already been loaded.
  for (size_t i = 0; i < mdeclv->size; ++i) {
    if (strcmp(mdeclv->xs[i]->name->name, name) == 0) {
      return mdeclv->xs[i];
    }
  }

  char* filename = FindModuleFile(arena, path, name, ".mdecl");
  if (filename == NULL) {
    fprintf(stderr, "unable to locate %s.mdecl on search path\n", name);
    return NULL;
  }

  // Parse the module.
  FbldMDecl* mdecl = FbldParseMDecl(arena, filename);
  if (mdecl == NULL) {
    fprintf(stderr, "failed to parse module from %s\n", filename);
    return NULL;
  }

  assert(strcmp(mdecl->name->name, name) == 0);
  FblcVectorAppend(arena, *mdeclv, mdecl);

  // Load all modules that this one depends on.
  // TODO: detect and abort if the module module recursively depends on itself.
  for (size_t i = 0; i < mdecl->deps->size; ++i) {
    if (!FbldLoadMDecl(arena, path, mdecl->deps->xs[i]->name, mdeclv)) {
      fprintf(stderr, "failed to load module required by %s\n", name);
      return NULL;
    }
  }

  return mdecl;
}

// FbldLoadMDefn -- see documentation in fbld.h
FbldMDefn* FbldLoadMDefn(FblcArena* arena, FbldStringV* path, FbldName name, FbldMDeclV* mdeclv)
{
  char* filename = FindModuleFile(arena, path, name, ".mdefn");
  if (filename == NULL) {
    fprintf(stderr, "unable to locate %s.mdefn on search path\n", name);
    return NULL;
  }

  // Parse the module.
  FbldMDefn* mdefn = FbldParseMDefn(arena, filename);
  if (mdefn == NULL) {
    fprintf(stderr, "failed to parse module from %s\n", filename);
    return NULL;
  }

  assert(strcmp(mdefn->name->name, name) == 0);

  // Load all modules that this one depends on.
  // TODO: detect and abort if the module recursively depends on itself.
  for (size_t i = 0; i < mdefn->deps->size; ++i) {
    if (!FbldLoadMDecl(arena, path, mdefn->deps->xs[i]->name, mdeclv)) {
      fprintf(stderr, "failed to load module required by %s\n", name);
      return NULL;
    }
  }

  // Check that this definition is valid.
  if (!FbldCheckMDefn(mdeclv, mdefn)) {
    return NULL;
  }

  return mdefn;
}

// FbldLoadModules -- see documentation in fbld.h
bool FbldLoadModules(FblcArena* arena, FbldStringV* path, FbldName name, FbldMDeclV* mdeclv, FbldMDefnV* mdefnv)
{
  FbldMDefn* mdefn = FbldLoadMDefn(arena, path, name, mdeclv);
  if (mdefn == NULL) {
    return false;
  }

  FblcVectorAppend(arena, *mdefnv, mdefn);

  // Load all modules that this one depends on.
  // TODO: detect and abort if the module recursively depends on itself.
  for (size_t i = 0; i < mdefn->deps->size; ++i) {
    if (!FbldLoadModules(arena, path, mdefn->deps->xs[i]->name, mdeclv, mdefnv)) {
      FbldReportError("failed to load %s\n", mdefn->deps->xs[i]->loc, mdefn->deps->xs[i]->name);
      return false;
    }
  }
  return true;
}
