// FblcMain.c --
//
//   The file implements the main entry point for the Fblc interpreter.

#include "FblcInternal.h"

#define EX_OK 0
#define EX_USAGE 64
#define EX_DATAERR 65
#define EX_NOINPUT 66

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
      "Usage: fblc FILE\n"
      "Evaluate 'main()' in the environment of the fblc program FILE.\n"  
      "Example: fblc foo.fblc\n"
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
  ENABLE_LEAK_DETECTION();
  MALLOC_INIT();

  const char* filename = NULL;
  for (int i = 1; i < argc; i++) {
    if (strcmp("--help", argv[i]) == 0) {
      PrintUsage(stdout);
      return EX_OK;
    } else {
      if (filename != NULL) {
        fprintf(stderr, "multiple FILEs are not allowed.\n");
        PrintUsage(stderr);
        return EX_USAGE;
      }
      filename = argv[i];
    }
  }

  if (filename == NULL) {
    fprintf(stderr, "no input file.\n");
    return EX_USAGE;
  }

  FblcTokenStream toks;
  if (!FblcOpenFileTokenStream(&toks, filename)) {
    fprintf(stderr, "failed to open input FILE %s.\n", filename);
    return EX_NOINPUT;
  }

  FblcAllocator alloc;
  FblcInitAllocator(&alloc);
  FblcEnv* env = FblcParseProgram(&alloc, &toks);
  FblcCloseTokenStream(&toks);
  if (env == NULL) {
    fprintf(stderr, "failed to parse input FILE.\n");
    FblcFreeAll(&alloc);
    return EX_DATAERR;
  }

  if (!FblcCheckProgram(env)) {
    fprintf(stderr, "input FILE is not a well formed Fblc program.\n");
    FblcFreeAll(&alloc);
    return EX_DATAERR;
  }

  FblcProc* proc = FblcLookupProc(env, "main");
  if (proc == NULL) {
    FblcFunc* func = FblcLookupFunc(env, "main");
    if (func == NULL) {
      FblcFreeAll(&alloc);
      fprintf(stderr, "failed to find 'main' function.\n");
      return EX_USAGE;
    }

    // Make a proc wrapper for the function.
    FblcEvalActn* body = FblcAlloc(&alloc, sizeof(FblcEvalActn));
    body->tag = FBLC_EVAL_ACTN;
    body->loc = func->body->loc;
    body->expr = func->body;

    proc = FblcAlloc(&alloc, sizeof(FblcProc));
    proc->name = func->name;
    proc->return_type = func->return_type;
    proc->body = (FblcActn*)body;
    proc->portc = 0;
    proc->portv = NULL;
    proc->argc = func->argc;
    proc->argv = func->argv;
  }

  if (proc->portc != 0) {
    FblcFreeAll(&alloc);
    fprintf(stderr, "main process does not have 0 ports.\n");
    return EX_USAGE;
  }

  if (proc->argc != 0) {
    FblcFreeAll(&alloc);
    fprintf(stderr, "main process does not take 0 arguments.\n");
    return EX_USAGE;
  }

  FblcValue* value = FblcExecute(env, proc, NULL, NULL);
  FblcPrintValue(stdout, value);
  printf("\n");
  FblcRelease(value);
  FblcFreeAll(&alloc);
  CHECK_FOR_LEAKS();
  return EX_OK;
}
