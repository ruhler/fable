// load.c --
//   This file implements routines for loading fbld programs.

#include <assert.h>      // for assert
#include <stdio.h>       // for fprintf, stderr
#include <string.h>      // for strlen, strcpy, strcat
#include <sys/types.h>   // for stat
#include <sys/stat.h>    // for stat
#include <unistd.h>      // for stat

#include "fblc.h"
#include "fbld.h"

static bool ParseSubModules(FblcArena* arena, const char* dir, FbldProgram* prgm);
static bool ParseTopModule(FblcArena* arena, const char* dir, FbldQRef* qref, FbldProgram* prgm);
static FbldProgram* LoadParsedProgram(FblcArena* arena, const char* path, FbldQRef* entry);

// ParseSubModules --
//   Recursively parse all requisite submodules of the given program.
//
// Inputs:
//   arena - arena to use for allocations.
//   dir - the directory to use to find submodules. May be NULL, in which case
//         submodules are not allowed to be specified in a different file.
//   prgm - the program to parse the submodules for.
//
// Result:
//   True if the submodules were successfully parsed, false otherwise.
//
// Side effects:
//   Allocations for the program.
//   Reads from files beneath dir to get the content of necessary submodules.
//   Prints a message to stderr in case of error.
static bool ParseSubModules(FblcArena* arena, const char* dir, FbldProgram* prgm)
{
  for (size_t i = 0; i < prgm->declv->size; ++i) {
    if (prgm->declv->xs[i]->tag == FBLD_MODULE_DECL) {
      FbldModule* module = (FbldModule*)prgm->declv->xs[i];

      const char* sdir = (dir == NULL) ? "" : dir;

      char ndir[strlen(sdir) + 1 + strlen(module->_base.name->name) + 1];
      strcpy(ndir, sdir);
      strcat(ndir, "/");
      strcat(ndir, module->_base.name->name);

      if (module->_base.alias == NULL) {
        if (module->body == NULL) {
          if (dir == NULL) {
            FbldReportError("No implementation found for module %s\n",
                module->_base.name->loc, module->_base.name->name);
            return false;
          }

          char file[strlen(ndir) + strlen(".fbld") + 1];
          strcpy(file, ndir);
          strcat(file, ".fbld");

          module->body = FbldParseProgram(arena, file);
          if (module->body == NULL) {
            FbldReportError("Error parsing implementation of module %s\n",
                module->_base.name->loc, module->_base.name->name);
            return false;
          }
        }

        if (!ParseSubModules(arena, dir == NULL ? NULL : ndir, module->body)) {
          return false;
        }
      }
    }
  }
  return true;
}

// ParseTopModule --
//   Parse a top level module if needed.
//
// Inputs:
//   arena - arena for allocations.
//   dir - the top level directory.
//   qref - (unresolved) qref referencing the top level module.
//   prgm - the program to add the parsed module to.
//
// Result:
//   true if the module was successfully added to prgm if needed, false
//   otherwise.
//
// Side effects:
//   If needed, parses the module and appends it to prgm.
//   Prints a message to stderr on error.
static bool ParseTopModule(FblcArena* arena, const char* dir, FbldQRef* qref, FbldProgram* prgm)
{
  // Extract the name of the module from the qref.
  assert(qref != NULL && "TODO: How to deal with this?");
  while (qref->mref != NULL) {
    qref = qref->mref;
  }

  // Check to see if we already have the module loaded.
  for (size_t i = 0; i < prgm->declv->size; ++i) {
    if (FbldNamesEqual(prgm->declv->xs[i]->name->name, qref->name->name)) {
      return true;
    }
  }

  // Parse the module.
  char ndir[strlen(dir) + 1 + strlen(qref->name->name) + 1];
  strcpy(ndir, dir);
  strcat(ndir, "/");
  strcat(ndir, qref->name->name);

  char file[strlen(ndir) + strlen(".fbld") + 1];
  strcpy(file, ndir);
  strcat(file, ".fbld");

  FbldProgram* body = FbldParseProgram(arena, file);
  if (body == NULL) {
    FbldReportError("Unable to load implementation of module %s\n",
        qref->name->loc, qref->name->name);
    return false;
  }

  if (!ParseSubModules(arena, ndir, body)) {
    return false;
  }

  FbldModule* module = FBLC_ALLOC(arena, FbldModule);
  module->_base.tag = FBLD_MODULE_DECL;

  // TODO: What to use for the name->loc of the module?
  // TODO: Can we assume name will stay around?
  module->_base.name = qref->name;

  module->_base.paramv = FBLC_ALLOC(arena, FbldDeclV);
  module->_base.access = FBLD_PUBLIC_ACCESS;
  module->_base.alias = NULL;
  FblcVectorInit(arena, *module->_base.paramv);
  module->iref = NULL;
  module->body = body;

  FblcVectorAppend(arena, *prgm->declv, &module->_base);
  return true;
}

// LoadParsedProgram --
//   Load a parsed program from file or directory as appropriate.
//
// Inputs:
//   arena - arena to use for allocations.
//   path - the name of a file or directory describing the program to load.
//   entry - optional (unresolved) entry to guide what files need to be parsed
//           if path is a directory.
//
// Results:
//   The parsed program, or NULL on error. If path is a directory, as much of
//   the files within path will be parsed to cover everything needed for the
//   given entry. Type checking is not performed.
//
// Side effects:
//   Allocations for the program. Prints an error message to stderr in case of
//   error.
//
// TODO: Better cleanup of allocations on error.
static FbldProgram* LoadParsedProgram(FblcArena* arena, const char* path, FbldQRef* entry)
{
  struct stat sb;
  if (stat(path, &sb) == -1) {
    fprintf(stderr, "Unable to stat %s\n", path);
    return NULL;
  }

  if (S_ISDIR(sb.st_mode)) {
    FbldProgram* prgm = FBLC_ALLOC(arena, FbldProgram);
    prgm->importv = FBLC_ALLOC(arena, FbldImportV);
    prgm->declv = FBLC_ALLOC(arena, FbldDeclV);
    FblcVectorInit(arena, *prgm->importv);
    FblcVectorInit(arena, *prgm->declv);
    if (!ParseTopModule(arena, path, entry, prgm)) {
      return NULL;
    }

    // Note: prgm->declv will continue to grow as this loop iterates.
    for (size_t m = 0; m < prgm->declv->size; ++m) {
      // See what other top level modules we need to import.
      assert(prgm->declv->xs[m]->tag == FBLD_MODULE_DECL);
      FbldModule* module = (FbldModule*)prgm->declv->xs[m];
      for (size_t i = 0; i < module->body->importv->size; ++i) {
        FbldImport* import = module->body->importv->xs[i];
        if (import->mref == NULL) {
          for (size_t e = 0; e < import->itemv->size; ++e) {
            FbldImportItem* item = import->itemv->xs[e];
            if (!ParseTopModule(arena, path, item->source, prgm)) {
              return NULL;
            }
          }
        }
      }
    }
    return prgm;
  }

  FbldProgram* prgm = FbldParseProgram(arena, path);
  if (prgm == NULL || !ParseSubModules(arena, NULL, prgm)) {
    return NULL;
  }
  return prgm;
}

// FbldLoadProgram -- see documentation in fbld.h
FbldProgram* FbldLoadProgram(FblcArena* arena, const char* path, FbldQRef* entry)
{
  FbldProgram* prgm = LoadParsedProgram(arena, path, entry);
  if (prgm == NULL) {
    return NULL;
  }

  if (!FbldCheckProgram(arena, prgm)) {
    return NULL;
  }
  return prgm;
}

// FbldLoadCompileProgram -- see documentation in fbld.h
FbldLoaded* FbldLoadCompileProgram(FblcArena* arena, FbldAccessLocV* accessv, const char* path, FbldQRef* entry)
{
  FbldProgram* prgm = LoadParsedProgram(arena, path, entry);
  if (prgm == NULL) {
    return NULL;
  }

  if (!FbldCheckProgram(arena, prgm)) {
    return NULL;
  }

  if (!FbldCheckQRef(arena, prgm, entry)) {
    return NULL;
  }
  return FbldCompileProgram(arena, accessv, prgm, entry);
}
