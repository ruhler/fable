// fble-deps.c --
//   The implementation of the fble-deps program, which generates gcc -MD
//   compatible dependencies for an .fble file.

#include <assert.h>   // for assert
#include <string.h>   // for strcmp, strlen
#include <stdio.h>    // for FILE, fprintf, stderr

#include "fble-alloc.h"   // for FbleFree.
#include "fble-load.h"    // for FbleLoad.
#include "fble-vector.h"  // for FbleVectorInit.

#define EX_SUCCESS 0
#define EX_FAIL 1
#define EX_USAGE 2

static void PrintUsage(FILE* stream);
int main(int argc, char* argv[]);

// PrintUsage --
//   Prints help info to the given output stream.
//
// Inputs:
//   stream - The output stream to write the usage information to.
//
// Result:
//   None.
//
// Side effects:
//   Outputs usage information to the given stream.
static void PrintUsage(FILE* stream)
{
  fprintf(stream, "%s",
      "Usage: fble-deps [OPTION...] -t TARGET -m MODULE_PATH\n"
      "\n"
      "Description:\n"
      "  Outputs a depfile suitable for use with ninja build specifying the\n"
      "  .fble files the given module depends on.\n"
      "\n"
      "Options:\n"
      "  -h, --help\n"
      "     Print this help message and exit.\n"
      "  -I DIR\n"
      "     Adds DIR to the module search path.\n"
      "  -t, --target TARGET\n"
      "     Specifies the name of the target to use in the generated depfile.\n"
      "  -m, --module MODULE_PATH\n"
      "     The path of the module to get dependencies for.\n"
      "\n"
      "Example:\n"
      "  fble-deps -I prgms -t Foo.fble.d -m /Foo% > Foo.fble.d\n"
  );
}

// main --
//   The main entry point for the fble-deps program.
//
// Inputs:
//   argc - The number of command line arguments.
//   argv - The command line arguments.
//
// Results:
//   0 on success, non-zero on error.
//
// Side effects:
//   Prints an error to stderr and exits the program in the case of error.
int main(int argc, char* argv[])
{
  FbleSearchPath search_path;
  FbleVectorInit(search_path);
  const char* target = NULL;
  const char* mpath_string = NULL;

  argc--;
  argv++;
  while (argc > 0) {
    if (strcmp("-h", argv[0]) == 0 || strcmp("--help", argv[0]) == 0) {
      PrintUsage(stdout);
      FbleFree(search_path.xs);
      return EX_SUCCESS;
    }

    if (strcmp("-I", argv[0]) == 0) {
      if (argc < 2) {
        fprintf(stderr, "Error: missing argument to -I option.\n");
        PrintUsage(stderr);
        FbleFree(search_path.xs);
        return EX_USAGE;
      }

      FbleVectorAppend(search_path, argv[1]);
      argc -= 2;
      argv += 2;
      continue;
    }

    if (strcmp("-t", argv[0]) == 0 || strcmp("--target", argv[0]) == 0) {
      if (argc < 2) {
        fprintf(stderr, "Error: missing argument to %s option.\n", argv[0]);
        PrintUsage(stderr);
        FbleFree(search_path.xs);
        return EX_USAGE;
      }

      target = argv[1];
      argc -= 2;
      argv += 2;
      continue;
    }

    if (strcmp("-m", argv[0]) == 0 || strcmp("--module", argv[0]) == 0) {
      if (argc < 2) {
        fprintf(stderr, "Error: missing argument to %s option.\n", argv[0]);
        PrintUsage(stderr);
        FbleFree(search_path.xs);
        return EX_USAGE;
      }

      mpath_string = argv[1];
      argc -= 2;
      argv += 2;
      continue;
    }

    if (argv[0][0] == '-') {
      fprintf(stderr, "Error: unrecognized option '%s'\n", argv[0]);
      PrintUsage(stderr);
      FbleFree(search_path.xs);
      return EX_USAGE;
    }

    fprintf(stderr, "Error: invalid argument '%s'\n", argv[0]);
    PrintUsage(stderr);
    FbleFree(search_path.xs);
    return EX_USAGE;
  }

  if (target == NULL) {
    fprintf(stderr, "missing required --target option.\n");
    PrintUsage(stderr);
    FbleFree(search_path.xs);
    return EX_USAGE;
  }

  if (mpath_string == NULL) {
    fprintf(stderr, "missing required --module option.\n");
    PrintUsage(stderr);
    FbleFree(search_path.xs);
    return EX_USAGE;
  }

  FbleModulePath* mpath = FbleParseModulePath(mpath_string);
  if (mpath == NULL) {
    FbleFree(search_path.xs);
    return EX_FAIL;
  }

  FbleStringV deps;
  FbleVectorInit(deps);
  FbleLoadedProgram* prgm = FbleLoad(search_path, mpath, &deps);
  FbleFree(search_path.xs);
  FbleFreeModulePath(mpath);
  FbleFreeLoadedProgram(prgm);

  if (prgm != NULL) {
    FbleSaveBuildDeps(stdout, target, deps);
  }

  for (size_t i = 0; i < deps.size; ++i) {
    FbleFreeString(deps.xs[i]);
  }
  FbleFree(deps.xs);

  return prgm == NULL ? EX_FAIL : EX_SUCCESS;
}
