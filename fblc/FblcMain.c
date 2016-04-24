// FblcMain.c --
//
//   The file implements the main entry point for the Fblc interpreter.

#include "FblcInternal.h"

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
      "Usage: fblc [--expect-error] FILE\n"
      "Evaluate 'main()' in the environment of the fblc program FILE.\n"  
      "Example: fblc foo.fblc\n"
      "\n"
      "Options:\n"
      "   --expect-error\n"
      "     If present, fblc exits with code 0 for malformed input programs,\n"
      "     and non-zero otherwise.\n"
  );
}

// main --
//
//   The main entry point for the fblc interpreter. Evaluates the expression
//   'main()' in the environment of the given fblc program and prints the
//   resulting value to standard output.
//
// Inputs:
//   argc - The number of command line arguments.
//   argv - The command line arguments.
//
// Results:
//   Zero on success, non-zero on error.
//
// Side effects:
//   Prints the value resulting from the evaluation of 'main()' to standard
//   out, or prints an error message to standard error if an error is
//   encountered.

int main(int argc, char* argv[])
{
  const char* filename = NULL;
  bool expect_error = false;
  for (int i = 1; i < argc; i++) {
    if (strcmp("--help", argv[i]) == 0) {
      PrintUsage(stdout);
      return 0;
    } else if (strcmp("--expect-error", argv[i]) == 0) {
      expect_error = true;
    } else {
      if (filename != NULL) {
        fprintf(stderr, "multiple FILEs are not allowed.\n");
        PrintUsage(stderr);
        return 1;
      }
      filename = argv[i];
    }
  }

  if (filename == NULL) {
    fprintf(stderr, "no input file.\n");
    return 1;
  }

  GC_INIT();

  FblcTokenStream* toks = FblcOpenTokenStream(filename);
  if (toks == NULL) {
    fprintf(stderr, "failed to open input FILE %s.\n", filename);
    return 1;
  }

  FblcEnv* env = FblcParseProgram(toks);
  FblcCloseTokenStream(toks);
  if (env == NULL) {
    fprintf(stderr, "failed to parse input FILE.\n");
    return expect_error ? 0 : 1;
  }

  if (!FblcCheckProgram(env)) {
    fprintf(stderr, "input FILE is not a well formed Fblc program.\n");
    return expect_error ? 0 : 1;
  }

  FblcFunc* func = FblcLookupFunc(env, "main");
  if (func != NULL) {
    if (func->argc != 0) {
      fprintf(stderr, "main function does not take 0 arguments.\n");
      return 1;
    }

    FblcValue* value = FblcEvaluate(env, func->body);
    FblcPrintValue(stdout, value);
    printf("\n");
    return expect_error ? 1 : 0;
  }

  FblcProc* proc = FblcLookupProc(env, "main");
  if (proc != NULL) {
    if (proc->portc != 0) {
      fprintf(stderr, "main process does not have 0 ports.\n");
      return 1;
    }

    if (proc->argc != 0) {
      fprintf(stderr, "main process does not take 0 arguments.\n");
      return 1;
    }

    if (proc->return_type == NULL) {
      fprintf(stderr, "main process does not return a value.\n");
      return 1;
    }
    
    FblcValue* value = FblcExecute(env, proc->body);
    FblcPrintValue(stdout, value);
    printf("\n");
    return expect_error ? 1 : 0;
  }

  fprintf(stderr, "failed to find 'main' function.\n");
  return 1;
}
