// fble-native.c --
//   This file implements the main entry point for the fble-native program,
//   which compiles *.fble code to *.c code.

#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include "fble-alloc.h"         // for FbleFree.
#include "fble-compile.h"       // for FbleCompile, etc.
#include "fble-module-path.h"   // for FbleParseModulePath.
#include "fble-vector.h"        // for FbleVectorInit.

#define EX_SUCCESS 0
#define EX_FAIL 1
#define EX_USAGE 2

static void PrintUsage(FILE* stream);

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
      "Usage: fble-compile [OPTIONS...] -m MODULE_PATH\n"
      "\n"
      "Description:\n"
      "  Compiles an fble module to assembly code.\n"
      "\n"
      "Options:\n"
      "  -h, --help\n"
      "    Print this help message and exit.\n"
      "  -I DIR\n"
      "    Add DIR to the module search path.\n"
      "  -m, --module MODULE_PATH\n"
      "     The path of the module to get dependencies for.\n"
      "  -c, --compile\n"
      "    Compile the module to assembly.\n"
      "  -e, --export NAME\n"
      "    Generate a function with the given NAME to export the module.\n"
      "\n"
      "  At least one of --compile or --export must be provided.\n"
      "\n"
      "Exit Status:\n"
      "  0 on success.\n"
      "  1 on failure.\n"
      "  2 on usage error.\n"
      "\n"
      "Example:\n"
      "  fble-compile -c -I prgms -m /Foo% > Foo.fble.s\n"
      "  fble-compile -e FbleCompiledMain -m /Foo% > Foo.fble.main.s\n"
  );
}

// main --
//   The main entry point for the fble-disassemble program.
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
  bool compile = false;
  const char* export = NULL;
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

    if (strcmp("-m", argv[0]) == 0 || strcmp("--module", argv[0]) == 0) {
      if (argc < 2) {
        fprintf(stderr, "Error: missing argument to %s option.\n", argv[0]);
        PrintUsage(stderr);
        FbleFree(search_path.xs);
        return EX_USAGE;
      }

      if (mpath_string != NULL) {
        fprintf(stderr, "Error: duplicate --module options.\n");
        PrintUsage(stderr);
        FbleFree(search_path.xs);
        return EX_USAGE;
      }

      mpath_string = argv[1];
      argc -= 2;
      argv += 2;
      continue;
    }

    if (strcmp("-c", argv[0]) == 0 || strcmp("--compile", argv[0]) == 0) {
      compile = true;
      argc--;
      argv++;
      continue;
    }

    if (strcmp("-e", argv[0]) == 0 || strcmp("--export", argv[0]) == 0) {
      if (argc < 2) {
        fprintf(stderr, "Error: missing argument to %s option.\n", argv[0]);
        PrintUsage(stderr);
        FbleFree(search_path.xs);
        return EX_USAGE;
      }

      if (export != NULL) {
        fprintf(stderr, "Error: duplicate --export options.\n");
        PrintUsage(stderr);
        FbleFree(search_path.xs);
        return EX_USAGE;
      }

      export = argv[1];
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

  if (!compile && export == NULL) {
    fprintf(stderr, "one of --export NAME or --compile must be specified.\n");
    PrintUsage(stderr);
    FbleFree(search_path.xs);
    return EX_USAGE;
  }

  FbleModulePath* mpath = FbleParseModulePath(mpath_string);
  if (mpath == NULL) {
    FbleFree(search_path.xs);
    return EX_FAIL;
  }

  if (export != NULL) {
    FbleGenerateAArch64Export(stdout, export, mpath);
  }

  if (compile) {
    FbleLoadedProgram* prgm = FbleLoad(search_path, mpath, NULL);
    if (prgm == NULL) {
      FbleFreeModulePath(mpath);
      return EX_FAIL;
    }

    FbleCompiledModule* module = FbleCompileModule(prgm);
    FbleFreeLoadedProgram(prgm);

    if (module == NULL) {
      FbleFreeModulePath(mpath);
      return EX_FAIL;
    }

    FbleFreeModulePath(module->path);
    module->path = FbleCopyModulePath(mpath);

    FbleGenerateAArch64(stdout, module);

    FbleFreeCompiledModule(module);
  }

  FbleFree(search_path.xs);
  FbleFreeModulePath(mpath);
  return EX_SUCCESS;
}
