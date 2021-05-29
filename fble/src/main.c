// main.c --
//   Provides the implementation of FbleMain.

#include "fble-main.h"

#include "fble-link.h"

// FbleMain -- 
//   See documentation in fble-main.h
FbleValue* FbleMain(FbleValueHeap* heap, FbleProfile* profile, FbleCompiledModuleFunction* compiled_main, int argc, char** argv)
{
  if (compiled_main != NULL) {
    return FbleLinkFromCompiled(compiled_main, heap, profile);
  }

  if (argc < 1) {
    fprintf(stderr, "no search path provided.\n");
    return NULL;
  }
  const char* search_path = argv[0];

  if (argc < 2) {
    fprintf(stderr, "no module path provided.\n");
    return NULL;
  }
  const char* mpath_string = argv[1];

  FbleModulePath* mpath = FbleParseModulePath(mpath_string);
  if (mpath == NULL) {
    return NULL;
  }

  FbleValue* linked = FbleLinkFromSource(heap, search_path, mpath, profile);
  FbleFreeModulePath(mpath);
  return linked;
}

