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
      "Usage: fblcbi PROGRAM MAIN [ARG...] \n"
      "Evaluate the function or process with id MAIN in the environment of the\n"
      "fblc program PROGRAM with the given ARGs.\n"  
      "PROGRAM should be a sequence of digits '0' and '1' representing the program.\n"
      "MAIN should be the decimal integer id of the function to execute.\n"
      "ARG should be a sequence of digits '0' and '1' representing an argument value.\n"
      "The number of arguments must match the expected number for the MAIN\n"
      "function.\n"
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
    fprintf(stderr, "no input program.\n");
    return 1;
  }

  if (argc <= 2) {
    fprintf(stderr, "no main entry point provided.\n");
    return 1;
  }

  const char* program_bits = argv[1];
  size_t entry = atoi(argv[2]);

  InputBitStream bits;
  OpenBinaryStringInputBitStream(&bits, program_bits);

  Allocator alloc;
  InitAllocator(&alloc);
  Program* program = DecodeProgram(&alloc, &bits);

  if (entry >= program->declc) {
    fprintf(stderr, "invalid entry id: %zi.\n", entry);
    return 1;
  }

  Decl* decl = program->declv[entry];
  if (decl->tag != FUNC_DECL) {
    fprintf(stderr, "entry %zi is not a function.\n", entry);
    return 1;
  }

  FuncDecl* func_decl = (FuncDecl*)decl;
  if (func_decl->argc != 0) {
    fprintf(stderr, "Error: TODO: support arguments to main function.\n");
    return 1;
  }

  Value* value = Execute(program, func_decl);
  assert(value != NULL);

  OutputBitStream output;
  OpenBinaryOutputBitStream(&output, STDOUT_FILENO);
  EncodeValue(&output, program, func_decl->return_type, value);
  Release(value);

  FreeAll(&alloc);
  CHECK_FOR_LEAKS();
  return 0;
}
