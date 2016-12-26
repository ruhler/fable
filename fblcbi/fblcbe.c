// fblcbe.c --
//
//   The file implements the main entry point for the fblc binary encoder.

#include "Internal.h"

static void PrintUsage(FILE* fout);

// PrintUsage --
//   
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
  fprintf(stream,
      "Usage: fblcbe FILE\n"
      "Encode the fblc program FILE in binary format.\n"
  );
}

// main --
//
//   The main entry point for the fblc binary encoder.
//
// Inputs:
//   argc - The number of command line arguments.
//   argv - The command line arguments.
//
// Results:
//   0 on success, non-zero on error.
//
// Side effects:
//   Prints the binary encoding of the given program to standard out, or
//   prints an error message to standard error if an error is encountered.

int main(int argc, char* argv[])
{
  ENABLE_LEAK_DETECTION();
  MALLOC_INIT();

  if (argc > 1 && strcmp("--help", argv[1]) == 0) {
    PrintUsage(stdout);
    return 0;
  }

  if (argc <= 1) {
    fprintf(stderr, "no input file.\n");
    return 1;
  }

  const char* filename = argv[1];

  TokenStream toks;
  if (!OpenFileTokenStream(&toks, filename)) {
    fprintf(stderr, "failed to open input FILE %s.\n", filename);
    return 1;
  }

  Allocator alloc;
  InitAllocator(&alloc);
  Env* env = ParseProgram(&alloc, &toks);
  CloseTokenStream(&toks);
  if (env == NULL) {
    fprintf(stderr, "failed to parse input FILE.\n");
    FreeAll(&alloc);
    return 1;
  }

  if (!CheckProgram(env)) {
    fprintf(stderr, "input FILE is not a well formed  program.\n");
    FreeAll(&alloc);
    return 1;
  }

  OutputBitStream output;
  OpenBinaryOutputBitStream(&output, STDOUT_FILENO);
  EncodeProgram(&output, env);

  FreeAll(&alloc);
  CHECK_FOR_LEAKS();
  return 0;
}
