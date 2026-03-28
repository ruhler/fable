/**
 * @file module-path.c
 *  FbleModulePath routines.
 */

#include <fble/fble-module-path.h>

#include <assert.h>     // for assert
#include <ctype.h>      // for isalnum
#include <string.h>     // for strlen, strcat

#include <fble/fble-alloc.h>
#include <fble/fble-vector.h>

static FbleString* Mangle(FbleModulePath* path, const char* name);


/**
 * @func[Mangle] Performs name mangling.
 *  Shared helper function for FbleMangleModulePath and
 *  FbleMangleForeignName depending on whether name is NULL or non-NULL.
 *
 *  @arg[FbleModulePath*][path] The module path.
 *  @arg[const char*][name] The name of the foreign function or NULL.
 *
 *  @returns[FbleString*] The mangled name.
 */
static FbleString* Mangle(FbleModulePath* path, const char* name)
{
  // Determine the length of the name.
  size_t len = strlen("_Fble") + 1; // prefix and terminating '\0'.
  for (size_t i = 0; i < path->path.size; ++i) {
    len += 4;   // translated '/' character.
    for (const char* p = path->path.xs[i].name->str; *p != '\0'; p++) {
      if (isalnum((unsigned char)*p)) {
        len++;        // untranslated character
      } else {
        len += 4;     // translated character
      }
    }
  }
  len += 4; // translated '%' character.

  if (name != NULL) {
    len += 4;   // translate '.' character.
    for (const char* p = name; *p != '\0'; p++) {
      if (isalnum((unsigned char)*p)) {
        len++;        // untranslated character
      } else {
        len += 4;     // translated character
      }
    }
  }

  // Construct the name.
  char data[len];
  char translated[5];
  data[0] = '\0';
  strcat(data, "_Fble");
  for (size_t i = 0; i < path->path.size; ++i) {
    sprintf(translated, "_%02x_", '/');
    strcat(data, translated);
    for (const char* p = path->path.xs[i].name->str; *p != '\0'; p++) {
      if (isalnum((unsigned char)*p)) {
        sprintf(translated, "%c", *p);
        strcat(data, translated);
      } else {
        sprintf(translated, "_%02x_", *p);
        strcat(data, translated);
      }
    }
  }
  sprintf(translated, "_%02x_", '%');
  strcat(data, translated);

  if (name != NULL) {
    sprintf(translated, "_%02x_", '.');
    strcat(data, translated);
    for (const char* p = name; *p != '\0'; p++) {
      if (isalnum((unsigned char)*p)) {
        sprintf(translated, "%c", *p);
        strcat(data, translated);
      } else {
        sprintf(translated, "_%02x_", *p);
        strcat(data, translated);
      }
    }
  }

  return FbleNewString(data);
}

// FbleNewModulePath -- see documentation in fble-module-path.h
FbleModulePath* FbleNewModulePath(FbleLoc loc)
{
  FbleModulePath* path = FbleAlloc(FbleModulePath);
  path->refcount = 1;
  path->magic = FBLE_MODULE_PATH_MAGIC;
  path->loc = FbleCopyLoc(loc);
  FbleInitVector(path->path);
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
    FbleFreeVector(path->path);
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

// See documentation in fble-module-path.h
FbleString* FbleMangleModulePath(FbleModulePath* path)
{
  return Mangle(path, NULL);
}

// See documentation in fble-module-path.h
FbleString* FbleMangleForeignName(FbleModulePath* path, const char* name)
{
  return Mangle(path, name);
}
