
#include "fble-module-path.h"

#include <assert.h>     // for assert
#include <string.h>     // for strlen, strcat

#include "fble-alloc.h"
#include "fble-vector.h"

// FbleNewModulePath -- see documentation in fble-module-path.h
FbleModulePath* FbleNewModulePath(FbleLoc loc)
{
  FbleModulePath* path = FbleAlloc(FbleModulePath);
  path->refcount = 1;
  path->magic = FBLE_MODULE_PATH_MAGIC;
  path->loc = FbleCopyLoc(loc);
  FbleVectorInit(path->path);
  return path;
}

// FbleCopyModulePath -- see documentation in fble-module-path.h
FbleModulePath* FbleCopyModulePath(FbleModulePath* path)
{
  path->refcount++;
  return path;
}

// FbleFreeModulePath -- see documentation in fble-module-path.h
void FbleFreeModulePath(FbleModulePath* path)
{
  assert(path->magic == FBLE_MODULE_PATH_MAGIC && "corrupt FbleModulePath");
  if (--path->refcount == 0) {
    FbleFreeLoc(path->loc);
    for (size_t i = 0; i < path->path.size; ++i) {
      FbleFreeName(path->path.xs[i]);
    }
    FbleFree(path->path.xs);
    FbleFree(path);
  }
}

// FbleModulePathName -- see documentation in fble-module-path.h
FbleName FbleModulePathName(FbleModulePath* path)
{
  size_t len = 3;   // We at least need 3 chars: '/', '%', '\0'
  for (size_t i = 0; i < path->path.size; ++i) {
    len += 1 + strlen(path->path.xs[i].name->str);
  }

  FbleString* string = FbleAllocExtra(FbleString, len);
  string->refcount = 1;
  string->magic = FBLE_STRING_MAGIC;
  FbleName name = {
    .name = string,
    .loc = FbleCopyLoc(path->loc),
    .space = FBLE_NORMAL_NAME_SPACE
  };

  string->str[0] = '\0';
  if (path->path.size == 0) {
    strcat(string->str, "/");
  }
  for (size_t i = 0; i < path->path.size; ++i) {
    strcat(string->str, "/");
    strcat(string->str, path->path.xs[i].name->str);
  }
  strcat(string->str, "%");
  return name;
}

// FblePrintModulePath -- see documentation in fble-module-path.h
void FblePrintModulePath(FILE* fout, FbleModulePath* path)
{
  if (path->path.size == 0) {
    fprintf(fout, "/");
  }
  for (size_t i = 0; i < path->path.size; ++i) {
    // TODO: Use quotes if the name contains any special characters, and
    // escape quotes as needed so the user can distinguish, for example,
    // between /Foo/Bar% and /'Foo/Bar'%.
    fprintf(fout, "/%s", path->path.xs[i].name->str);
  }
  fprintf(fout, "%%");
}

// FbleModulePathsEqual -- see documentation in fble-module-path.h
bool FbleModulePathsEqual(FbleModulePath* a, FbleModulePath* b)
{
  if (a->path.size != b->path.size) {
    return false;
  }

  for (size_t i = 0; i < a->path.size; ++i) {
    if (!FbleNamesEqual(a->path.xs[i], b->path.xs[i])) {
      return false;
    }
  }
  return true;
}

// FbleModuleBelongsToPackage -- see documentation in fble-module-path.h
bool FbleModuleBelongsToPackage(FbleModulePath* module, FbleModulePath* package)
{
  if (module->path.size < package->path.size) {
    return false;
  }

  for (size_t i = 0; i < package->path.size; ++i) {
    if (!FbleNamesEqual(module->path.xs[i], package->path.xs[i])) {
      return false;
    }
  }
  return true;
}
