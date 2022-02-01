// main.c --
//   Provides the implementation of FbleMain.

#include "fble-main.h"

#include <string.h>       // for strcmp

#include "fble-alloc.h"
#include "fble-link.h"
#include "fble-vector.h"

// FbleMain -- 
//   See documentation in fble-main.h
FbleValue* FbleMain(FbleValueHeap* heap, FbleProfile* profile, FbleCompiledModuleFunction* compiled_main, int argc, char** argv)
{
  if (compiled_main != NULL) {
    return FbleLinkFromCompiled(compiled_main, heap, profile);
  }

  FbleSearchPath search_path;
  FbleVectorInit(search_path);
  while (argc > 1 && strcmp(argv[0], "-I") == 0) {
    FbleVectorAppend(search_path, argv[1]);
    argc -= 2;
    argv += 2;
  }

  if (argc < 1) {
    fprintf(stderr, "no module path provided.\n");
    FbleFree(search_path.xs);
    return NULL;
  } else if (argc > 1) {
    fprintf(stderr, "too many arguments.\n");
    FbleFree(search_path.xs);
    return NULL;
  }
  const char* mpath_string = argv[0];

  FbleModulePath* mpath = FbleParseModulePath(mpath_string);
  if (mpath == NULL) {
    FbleFree(search_path.xs);
    return NULL;
  }

  FbleValue* linked = FbleLinkFromSource(heap, search_path, mpath, profile);
  FbleFree(search_path.xs);
  FbleFreeModulePath(mpath);
  return linked;
}
