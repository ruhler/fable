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
      "Usage: fblc FILE MAIN [PORT...] [ARG...] \n"
      "Evaluate the function or process called MAIN in the environment of the\n"
      "fblc program FILE with the given PORTs and ARGs.\n"  
      "PORT is the filename to use to read or write the port from.\n"
      "ARG is a value text representation of the argument value.\n"
      "The number and type of ports and arguments must match the expected\n"
      "types for the MAIN function or process.\n"
      "Example: fblc main in.txt 'Bool:true(Unit())'\n"
  );
}

// main --
//
//   The main entry point for the fblc interpreter. Evaluates the MAIN
//   function or process from the given program with the given ports and
//   arguments. The result is printed to standard output.
//
// Inputs:
//   argc - The number of command line arguments.
//   argv - The command line arguments.
//
// Results:
//   EX_OK success, EX_USAGE on user usage error, EX_DATAERR if the program is
//   not well formed, EX_NOINPUT if the input program could not be accessed.
//
// Side effects:
//   Prints the value resulting from the evaluation of the MAIN process on the
//   given arguments to standard out, or prints an error message to standard
//   error if an error is encountered.

int main(int argc, char* argv[])
{
  ENABLE_LEAK_DETECTION();
  MALLOC_INIT();

  if (argc > 1 && strcmp("--help", argv[1]) == 0) {
    PrintUsage(stdout);
    return EX_OK;
  }

  if (argc <= 1) {
    fprintf(stderr, "no input file.\n");
    return EX_USAGE;
  }

  if (argc <= 2) {
    fprintf(stderr, "no main entry point provided.\n");
    return EX_USAGE;
  }

  const char* filename = argv[1];
  const char* entry = argv[2];

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

  FblcProc* proc = FblcLookupProc(env, entry);
  if (proc == NULL) {
    FblcFunc* func = FblcLookupFunc(env, entry);
    if (func == NULL) {
      FblcFreeAll(&alloc);
      fprintf(stderr, "failed to find process or function '%s'.\n", entry);
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

  if (argc != 3 + proc->portc + proc->argc) {
    FblcFreeAll(&alloc);
    fprintf(stderr, "expected %i ports/args for %s, but %i were provided.\n",
        proc->portc + proc->argc, entry, argc-3);
    return EX_USAGE;
  }

  if (proc->portc != 0) {
    FblcFreeAll(&alloc);
    fprintf(stderr, "main process does not have 0 ports.\n");
    return EX_USAGE;
  }

  argv += 3;
  FblcValue* args[proc->argc];
  bool err = false;
  for (int i = 0; i < proc->argc; i++) {
    const char* str = *argv++;
    args[i] = NULL;
    FblcType* type = FblcLookupType(env, proc->argv[i].type.name);
    assert(type != NULL);
    FblcOpenStringTokenStream(&toks, str, str);
    args[i] = FblcParseValue(env, type, &toks);
    FblcCloseTokenStream(&toks);
    err = err || (args[i] == NULL);
  }

  if (!err) {
    FblcValue* value = FblcExecute(env, proc, NULL, args);
    FblcPrintValue(stdout, value);
    printf("\n");
    FblcRelease(value);
  }

  for (int i = 0; i < proc->argc; i++) {
    FblcRelease(args[i]);
  }
  FblcFreeAll(&alloc);
  CHECK_FOR_LEAKS();
  return err ? EX_USAGE : EX_OK;
}
