// load.c --
//   This file implements routines for loading fbld module declarations and
//   definitions.

#include <assert.h>   // for assert
#include <stdio.h>    // for sprintf, fprintf, stderr
#include <string.h>   // for strlen
#include <unistd.h>   // for access

#include "fblc.h"
#include "fbld.h"

static char* FindModuleFile(FblcArena* arena, FbldStringV* path, const char* name, const char* extension);


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
static char* FindModuleFile(FblcArena* arena, FbldStringV* path, const char* name, const char* extension)
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

// FbldLoadMType -- see documentation in fbld.h
FbldMType* FbldLoadMType(FblcArena* arena, FbldStringV* path, const char* name, FbldProgram* prgm)
{
  // Return the existing mtype declaration if it has already been loaded.
  for (size_t i = 0; i < prgm->mtypev.size; ++i) {
    if (FbldNamesEqual(name, prgm->mtypev.xs[i]->name->name)) {
      return prgm->mtypev.xs[i];
    }
  }

  char* filename = FindModuleFile(arena, path, name, ".mtype");
  if (filename == NULL) {
    fprintf(stderr, "unable to locate %s.mtype on search path\n", name);
    return NULL;
  }

  // Parse the module.
  FbldMType* mtype = FbldParseMType(arena, filename);
  if (mtype == NULL) {
    fprintf(stderr, "failed to parse module from %s\n", filename);
    return NULL;
  }

  assert(FbldNamesEqual(mtype->name->name, name));
  FblcVectorAppend(arena, prgm->mtypev, mtype);

  // Check that this declaration is valid.
  // TODO: detect and abort if the module module recursively depends on itself.
  if (!FbldCheckMType(arena, path, mtype, prgm)) {
    return NULL;
  }

  return mtype;
}

// FbldLoadMDecl -- see documentation in fbld.h
FbldMDefn* FbldLoadMDecl(FblcArena* arena, FbldStringV* path, const char* name, FbldProgram* prgm)
{
  // Return the existing declaration if it has already been loaded.
  for (size_t i = 0; i < prgm->mdeclv.size; ++i) {
    if (FbldNamesEqual(name, prgm->mdeclv.xs[i]->name->name)) {
      return prgm->mdeclv.xs[i];
    }
  }

  // Parse the module declaration if we haven't already.
  char* filename = FindModuleFile(arena, path, name, ".mdefn");
  if (filename == NULL) {
    fprintf(stderr, "unable to locate %s.mdefn on search path\n", name);
    return NULL;
  }

  FbldMDefn* mdecl = FbldParseMDefn(arena, filename);
  if (mdecl == NULL) {
    fprintf(stderr, "failed to parse module from %s\n", filename);
    return NULL;
  }
  assert(FbldNamesEqual(mdecl->name->name, name));
  FblcVectorAppend(arena, prgm->mdeclv, mdecl);

  // Check that this definition is valid.
  // TODO: detect and abort if the mdecl recursively depends on itself.
  if (!FbldCheckMDecl(arena, path, mdecl, prgm)) {
    return NULL;
  }
  return mdecl;
}

// FbldLoadMDefn -- see documentation in fbld.h
FbldMDefn* FbldLoadMDefn(FblcArena* arena, FbldStringV* path, const char* name, FbldProgram* prgm)
{
  // Return the existing mdefn declaration if it has already been loaded.
  for (size_t i = 0; i < prgm->mdefnv.size; ++i) {
    if (FbldNamesEqual(name, prgm->mdefnv.xs[i]->name->name)) {
      return prgm->mdefnv.xs[i];
    }
  }

  FbldMDefn* mdefn = FbldLoadMDecl(arena, path, name, prgm);
  if  (mdefn == NULL) {
    return NULL;
  }

  assert(FbldNamesEqual(mdefn->name->name, name));
  FblcVectorAppend(arena, prgm->mdefnv, mdefn);

  // Check that this definition is valid.
  // TODO: detect and abort if the module recursively depends on itself.
  if (!FbldCheckMDefn(arena, path, mdefn, prgm)) {
    return NULL;
  }

  return mdefn;
}

// FbldLoadMDefns -- see documentation in fbld.h
bool FbldLoadMDefns(FblcArena* arena, FbldStringV* path, const char* name, FbldProgram* prgm)
{
  if (!FbldLoadMDefn(arena, path, name, prgm)) {
    return false;
  }

  // Load all the mdefns still left to load.
  // Note: More mdeclvs will be added as modules are loaded, which means that
  // prgm->mdeclv->size will often increase during the body of the loop.
  // Because elements are only ever appended to mdeclv, this should be okay.
  // TODO: detect and abort if the module recursively depends on itself.
  for (size_t i = 0; i < prgm->mdeclv.size; ++i) {
    if (!FbldLoadMDefn(arena, path, prgm->mdeclv.xs[i]->name->name, prgm)) {
      return false;
    }
  }
  return true;
}
