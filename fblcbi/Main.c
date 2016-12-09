// Main.c --
//
//   The file implements the main entry point for the Fblc binary interpreter.

#include "Internal.h"


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
      "Usage: fblcbi FILE MAIN [ARG...] \n"
      "Evaluate the function or process with id MAIN in the environment of the\n"
      "fblc program FILE with the given ARGs.\n"  
      "Ports should be provided by arranging for file descriptors 3, 4, ...\n"
      "to be open on which data for port 1, 2, ... can be read or written as\n"
      "appropriate.\n"
      "ARG is a binary representation of the argument value, such as b1001001.\n"
      "The number of arguments must match the expected number for the MAIN\n"
      "function or process.\n"
      "Example: fblcbi 17 3<in.port 4>out.port 'b1'\n"
  );
}

// main --
//
//   The main entry point for the fblc binary interpreter. Evaluates the MAIN
//   function or process from the given program with the given ports and
//   arguments. The result is printed to standard output.
//
// Inputs:
//   argc - The number of command line arguments.
//   argv - The command line arguments.
//
// Results:
//   0 on success, non-zero on error.
//
// Side effects:
//   Prints the value resulting from the evaluation of the MAIN process on the
//   given arguments to standard out using the binary text format, or prints
//   an error message to standard error if an error is encountered.

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

  if (argc <= 2) {
    fprintf(stderr, "no main entry point provided.\n");
    return 1;
  }

  const char* filename = argv[1];
  size_t entry = atoi(argv[2]);

  BitStream bits;
  if (!OpenFileBitStream(&bits, filename)) {
    fprintf(stderr, "failed to open input FILE %s.\n", filename);
    return 1;
  }

  Allocator alloc;
  InitAllocator(&alloc);
  Program* program = ParseProgram(&alloc, &bits);
  CloseBitStream(&bits);

  if (entry >= program->declc) {
    fprintf(stderr, "invalid entry id: %i.\n", entry);
    return 1;
  }

  Decl* decl = program->decls[entry];
  if (decl->tag != FUNC_DECL) {
    fprintf(stderr, "entry %i is not a function.\n", entry);
    return 1;
  }

  FuncDecl* func_decl = (FuncDecl*)decl;
  if (func_decl->args->typec != 0) {
    fprintf(stderr, "Error: TODO: support arguments to main function.\n");
    return 1;
  }

  Value* value = Execute(program, func_decl);
  assert(value != NULL);
  PrintValue(value);
  Release(value);

  FreeAll(&alloc);
  CHECK_FOR_LEAKS();
  return 0;
}
