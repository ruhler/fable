// main.c --
//   Provides the implementation of FbleMain.

#include "fble-main.h"

#include "fble-link.h"

// FbleMain -- 
//   See documentation in fble-main.h
FbleValue* FbleMain(FbleValueHeap* heap, FbleProfile* profile, FbleCompiledMainFunction* compiled_main, int argc, char** argv)
{
  if (compiled_main != NULL) {
    return compiled_main(heap, profile);
  }

  const char* file = argc < 1 ? NULL : argv[0];
  const char* dir = argc < 2 ? NULL : argv[1];
  return FbleLinkFromSource(heap, file, dir, profile);
}

