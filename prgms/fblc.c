// FblcMain.c --
//
//   The file implements the main entry point for the Fblc interpreter.

#define _POSIX_SOURCE     // for fdopen
#include "FblcInternal.h"

#define EX_OK 0
#define EX_USAGE 64
#define EX_DATAERR 65
#define EX_NOINPUT 66

typedef struct {
  FblcEnv* env;
  FblcType* type;
  FblcTokenStream toks;
} InputData;

typedef union {
  InputData input;
  FILE* output;
} UserData;

static void PrintUsage(FILE* fout);
static FblcValue* Input(InputData* user, FblcValue* value);
static FblcValue* Output(FILE* user, FblcValue* value);

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
      "Usage: fblc FILE MAIN [ARG...] \n"
      "Evaluate the function or process called MAIN in the environment of the\n"
      "fblc program FILE with the given ARGs.\n"  
      "Ports should be provided by arranging for file descriptors 3, 4, ...\n"
      "to be open on which data for port 1, 2, ... can be read or written as\n"
      "appropriate.\n"
      "ARG is a value text representation of the argument value.\n"
      "The number of arguments must match the expected types for the MAIN\n"
      "function or process.\n"
      "Example: fblc main 3<in.port 4>out.port 'Bool:true(Unit())'\n"
  );
}

// Input --
//
//   An FblcIO function for getting port values from a token stream.
//
// Inputs:
//   user - User data with the token stream and value type to read.
//   value - Ignored; must be NULL.
//
// Results:
//   The next value read from the stream, or NULL if there is no value
//   available.
//
// Side effects:
//   Advances the token stream to the next value if a value is ready. This
//   function should not block if no value is available.

static FblcValue* Input(InputData* user, FblcValue* value)
{
  assert(value == NULL);

  // TODO: Check if this would block before reading from the token stream.
  return FblcParseValue(user->env, user->type, &(user->toks));
}

// Output --
//
//   An FblcIO function for putting port values to a file stream.
//
// Inputs:
//   user - The file stream to output the value to.
//   value - The value to put to the stream.
//
// Results:
//   NULL.
//
// Side effects:
//   Prints the value to the file stream.

static FblcValue* Output(FILE* user, FblcValue* value)
{
  FblcPrintValue(user, value);
  fprintf(user, "\n");
  fflush(user);
  return NULL;
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

  argc -= 3;
  argv += 3;
  if (argc != proc->argc) {
    FblcFreeAll(&alloc);
    fprintf(stderr, "expected %i ports/args for %s, but %i were provided.\n",
        proc->portc + proc->argc, entry, argc);
    return EX_USAGE;
  }

  UserData user[proc->portc];
  FblcIO ios[proc->portc];

  for (int i = 0; i < proc->portc; i++) {
    int fd = i+3;
    if (proc->portv[i].polarity == FBLC_POLARITY_PUT) {
      user[i].output = fdopen(fd, "w");
      if  (user[i].output == NULL) {
        // TODO: What's the right error code to return in this case?
        fprintf(stderr, "unable to open fd %i for writing port %i\n", fd, i);
        return EX_NOINPUT;
      }
      ios[i].io = (FblcIOFunction)&Output;
      ios[i].user = user[i].output;
    } else {
      assert(proc->portv[i].polarity == FBLC_POLARITY_GET);
      user[i].input.env = env;
      user[i].input.type = FblcLookupType(env, proc->portv[i].type.name);
      assert(user[i].input.type != NULL);
      if (!FblcOpenFdTokenStream(&(user[i].input.toks), fd,
            proc->portv[i].name.name)) {
        fprintf(stderr, "unable to open fd %i for reading port %i\n", fd, i);
        return EX_NOINPUT;
      }
      ios[i].io = (FblcIOFunction)&Input;
      ios[i].user = &(user[i].input);
    }
  }

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
    FblcValue* value = FblcExecute(env, proc, ios, args);
    assert(value != NULL);
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
