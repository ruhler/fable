// load.c --
//   This file implements routines for loading fbld module declarations and
//   definitions.

#include <assert.h>   // for assert
#include <stdio.h>    // for sprintf, fprintf, stderr
#include <string.h>   // for strcmp, strlen
#include <unistd.h>   // for access

#include "fblc.h"
#include "fbld.h"

// FbldLoadMDecl -- see documentation in fbld.h
FbldMDecl* FbldLoadMDecl(FblcArena* arena, FbldStringV* path, const char* name, FbldMDeclV* mdeclv)
{
  // Return the existing module declaration if it has already been loaded.
  for (size_t i = 0; i < mdeclv->size; ++i) {
    if (strcmp(mdeclv->xs[i]->name->name, name) == 0) {
      return mdeclv->xs[i];
    }
  }

  // Locate the module declaration on the path.
  char* filename = NULL;
  for (size_t i = 0; filename == NULL && i < path->size; ++i) {
    size_t length = strlen(path->xs[i]) + 1 + strlen(name) + strlen(".mdecl") + 1;
    filename = arena->alloc(arena, sizeof(char) * length);
    sprintf(filename, "%s/%s.mdecl", path->xs[i], name);
    if (access(filename, F_OK) < 0) {
      arena->free(arena, filename);
      filename = NULL;
    }
  }

  if (filename == NULL) {
    fprintf(stderr, "unable to locate %s.mdecl on search path", name);
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
