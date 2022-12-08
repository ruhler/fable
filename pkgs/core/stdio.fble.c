
// stdio.fble.c --
//   Implementation of FbleStdio and FbleStdioMain functions.


#define _GNU_SOURCE     // for getline

#include "stdio.fble.h"

#include <stdio.h>      // for FILE, fprintf, fflush, getline
#include <stdlib.h>     // for free
#include <string.h>     // for strcmp

#include <fble/fble-alloc.h>       // for FbleFree
#include <fble/fble-arg-parse.h>   // for FbleParseBoolArg, etc.
#include <fble/fble-link.h>        // for FbleCompiledModuleFunction.
#include <fble/fble-value.h>       // for FbleValue, etc.
#include <fble/fble-vector.h>      // for FbleVectorInit.
#include <fble/fble-version.h>     // for FBLE_VERSION

#include "char.fble.h"        // for FbleCharValueAccess
#include "int.fble.h"         // for FbleIntValueAccess
#include "string.fble.h"      // for FbleNewStringValue, FbleStringValueAccess

#define EX_TRUE 0
#define EX_FALSE 1
#define EX_USAGE 2
#define EX_FAILURE 3

static void Output(FILE* stream, FbleValue* str);

static FbleExecStatus StdinImpl(
    FbleValueHeap* heap, FbleThread* thread,
    FbleExecutable* executable,
    FbleValue** locals, FbleValue** statics,
    FbleBlockId profile_block_offset);
static FbleExecStatus StdoutImpl(
    FbleValueHeap* heap, FbleThread* thread,
    FbleExecutable* executable,
    FbleValue** locals, FbleValue** statics,
    FbleBlockId profile_block_offset);
static FbleExecStatus StderrImpl(
    FbleValueHeap* heap, FbleThread* thread,
    FbleExecutable* executable,
    FbleValue** locals, FbleValue** statics,
    FbleBlockId profile_block_offset);

static void PrintUsage(FILE* stream, FbleCompiledModuleFunction* module);

// Output --
//   Output a string to the given output stream.
//
// Inputs:
//   stream - the stream to output to
//   str - the /Core/String%.String@ to write
//
// Side effects:
//   Outputs the string to the stream and flushes the stream.
static void Output(FILE* stream, FbleValue* str)
{
  char* chars = FbleStringValueAccess(str);
  fprintf(stream, "%s", chars);
  fflush(stream);
  FbleFree(chars);
}

// Stdin -- Implementation of stdin function.
//   IO@<Maybe@<String@>>
static FbleExecStatus StdinImpl(
    FbleValueHeap* heap, FbleThread* thread,
    FbleExecutable* executable,
    FbleValue** locals, FbleValue** statics,
    FbleBlockId profile_block_offset)
{
  (void)executable;
  (void)statics;
  (void)profile_block_offset;

  FbleValue* world = locals[0];

  // Read a line from stdin.
  char* line = NULL;
  size_t len = 0;
  ssize_t read = getline(&line, &len, stdin);
  FbleValue* ms;
  if (read < 0) {
    ms = FbleNewEnumValue(heap, 1);
  } else {
    FbleValue* charS = FbleNewStringValue(heap, line);
    ms = FbleNewUnionValue(heap, 0, charS);
    FbleReleaseValue(heap, charS);
  }
  free(line);

  FbleValue* result = FbleNewStructValue_(heap, 2, world, ms);
  FbleReleaseValue(heap, ms);

  FbleReleaseValue(heap, locals[0]);
  return FbleThreadReturn(heap, thread, result);
}

// Stdout -- Implementation of stdout function.
//   (String@, World@) { R@<Unit@>; }
static FbleExecStatus StdoutImpl(
    FbleValueHeap* heap, FbleThread* thread,
    FbleExecutable* executable,
    FbleValue** locals, FbleValue** statics,
    FbleBlockId profile_block_offset)
{
  (void)executable;
  (void)statics;
  (void)profile_block_offset;

  FbleValue* str = locals[0];
  FbleValue* world = locals[1];

  Output(stdout, str);

  FbleValue* unit = FbleNewStructValue_(heap, 0);
  FbleValue* result = FbleNewStructValue_(heap, 2, world, unit);
  FbleReleaseValue(heap, unit);

  FbleReleaseValue(heap, locals[0]);
  FbleReleaseValue(heap, locals[1]);
  return FbleThreadReturn(heap, thread, result);
}

// Stderr -- Implementation of stderr function.
//   (String@, World@) { R@<Unit@>; }
static FbleExecStatus StderrImpl(
    FbleValueHeap* heap, FbleThread* thread,
    FbleExecutable* executable,
    FbleValue** locals, FbleValue** statics,
    FbleBlockId profile_block_offset)
{
  (void)executable;
  (void)statics;
  (void)profile_block_offset;

  FbleValue* str = locals[0];
  FbleValue* world = locals[1];

  Output(stderr, str);

  FbleValue* unit = FbleNewStructValue_(heap, 0);
  FbleValue* result = FbleNewStructValue_(heap, 2, world, unit);
  FbleReleaseValue(heap, unit);

  FbleReleaseValue(heap, locals[0]);
  FbleReleaseValue(heap, locals[1]);
  return FbleThreadReturn(heap, thread, result);
}

// PrintUsage --
//   Prints help info for FbleStdioMain to the given output stream.
//
// Inputs:
//   stream - The output stream to write the usage information to.
//   module - Non-NULL if a compiled module is provided, NULL otherwise.
//
// Side effects:
//   Outputs usage information to the given stream.
static void PrintUsage(FILE* stream, FbleCompiledModuleFunction* module)
{
  fprintf(stream, "Usage: fble-stdio [OPTION...]%s ARGS\n",
      module == NULL ? " -m MODULE_PATH" : "");
  fprintf(stream, "%s",
      "\n"
      "Description:\n"
      "  Runs an fble stdio program.\n"
      "\n"
      "Options:\n"
      "  -h, --help\n"
      "     Print this help message and exit.\n"
      "  -v, --version\n"
      "     Print version information and exit.\n");
  if (module == NULL) {
    fprintf(stream, "%s",
      "  -I DIR\n"
      "     Adds DIR to the module search path.\n"
      "  -m, --module MODULE_PATH\n"
      "     The path of the module to get dependencies for.\n");
  }
  fprintf(stream, "%s",
      "  --profile FILE\n"
      "    Writes a profile of the test run to FILE\n"
      "  --\n"
      "    Indicates the end of options. Everything that follows is considered\n"
      "    ARGS. Normally the first unrecognized option is considered the start\n"
      "    of ARGS.\n"
      "\n"
      "Exit Status:\n"
      "  0 if Stdio@ process returns true.\n"
      "  1 if Stdio@ process returns false.\n"
      "  2 on usage error.\n"
      "  3 on other error.\n"
      "\n"
      "Example:\n");
  fprintf(stream, "%s%s%s",
      "  fble-stdio --profile foo.prof ",
      module == NULL ? "-I prgms -m /Foo% " : "",
      "arg1 arg2\n");
}

// FbleStdio -- see documentation in stdio.fble.h
FbleValue* FbleStdio(FbleValueHeap* heap, FbleProfile* profile, FbleValue* stdio, size_t argc, FbleValue** argv)
{
  FbleValue* func = FbleEval(heap, stdio, profile);
  if (func == NULL) {
    return NULL;
  }

  FbleName block_names[5];
  block_names[0].name = FbleNewString("stdin");
  block_names[0].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  block_names[1].name = FbleNewString("stdout");
  block_names[1].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  block_names[2].name = FbleNewString("stderr");
  block_names[2].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  FbleBlockId block_id = 0;
  if (profile != NULL) {
    FbleNameV names = { .size = 3, .xs = block_names };
    block_id = FbleProfileAddBlocks(profile, names);
  };
  FbleFreeName(block_names[0]);
  FbleFreeName(block_names[1]);
  FbleFreeName(block_names[2]);

  FbleExecutable* stdin_exe = FbleAlloc(FbleExecutable);
  stdin_exe->refcount = 0;
  stdin_exe->magic = FBLE_EXECUTABLE_MAGIC;
  stdin_exe->num_args = 1;
  stdin_exe->num_statics = 0;
  stdin_exe->num_locals = 1;
  stdin_exe->profile_block_id = block_id;
  stdin_exe->run = &StdinImpl;
  stdin_exe->on_free = &FbleExecutableNothingOnFree;
  FbleValue* fble_stdin = FbleNewFuncValue(heap, stdin_exe, 0, NULL);

  FbleExecutable* stdout_exe = FbleAlloc(FbleExecutable);
  stdout_exe->refcount = 0;
  stdout_exe->magic = FBLE_EXECUTABLE_MAGIC;
  stdout_exe->num_args = 2;
  stdout_exe->num_statics = 0;
  stdout_exe->num_locals = 2;
  stdout_exe->profile_block_id = block_id + 1;
  stdout_exe->run = &StdoutImpl;
  stdout_exe->on_free = &FbleExecutableNothingOnFree;
  FbleValue* fble_stdout = FbleNewFuncValue(heap, stdout_exe, 0, NULL);

  FbleExecutable* stderr_exe = FbleAlloc(FbleExecutable);
  stderr_exe->refcount = 0;
  stderr_exe->magic = FBLE_EXECUTABLE_MAGIC;
  stderr_exe->num_args = 2;
  stderr_exe->num_statics = 0;
  stderr_exe->num_locals = 2;
  stderr_exe->profile_block_id = block_id + 2;
  stderr_exe->run = &StderrImpl;
  stderr_exe->on_free = &FbleExecutableNothingOnFree;
  FbleValue* fble_stderr = FbleNewFuncValue(heap, stderr_exe, 0, NULL);

  FbleValue* argS = FbleNewEnumValue(heap, 1);
  for (size_t i = 0; i < argc; ++i) {
    FbleValue* argP = FbleNewStructValue_(heap, 2, argv[argc - i -1], argS);
    FbleReleaseValue(heap, argS);
    argS = FbleNewUnionValue(heap, 0, argP);
    FbleReleaseValue(heap, argP);
  }

  FbleValue* args[4] = { fble_stdin, fble_stdout, fble_stderr, argS };
  FbleValue* computation = FbleApply(heap, func, args, profile);
  FbleReleaseValue(heap, func);
  FbleReleaseValue(heap, args[0]);
  FbleReleaseValue(heap, args[1]);
  FbleReleaseValue(heap, args[2]);
  FbleReleaseValue(heap, args[3]);

  if (computation == NULL) {
    return NULL;
  }

  // computation has type IO@<Bool@>, which is (World@) { R@<Bool@>; }
  FbleValue* world = FbleNewStructValue_(heap, 0);
  FbleValue* result = FbleApply(heap, computation, &world, profile);
  FbleReleaseValue(heap, computation);
  if (result == NULL) {
    return NULL;
  }

  // result has type R@<Bool@>, which is *(s, x)
  FbleValue* value = FbleStructValueAccess(result, 1);
  FbleRetainValue(heap, value);
  FbleReleaseValue(heap, result);
  return value;
}

// FbleStdioMain -- See documentation in stdio.fble.h
int FbleStdioMain(int argc, const char** argv, FbleCompiledModuleFunction* module)
{
  // To ease debugging of FbleStdioMain programs, cause the following useful
  // functions to be linked in:
  (void)(FbleCharValueAccess);
  (void)(FbleIntValueAccess);
  (void)(FbleStringValueAccess);

  FbleSearchPath search_path;
  FbleVectorInit(search_path);
  const char* module_path = NULL;
  const char* profile_file = NULL;
  bool end_of_options = false;
  bool help = false;
  bool error = false;
  bool version = false;

  argc--;
  argv++;
  while (!(help || error || version) && !end_of_options && argc > 0) {
    if (FbleParseBoolArg("-h", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--help", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("-v", &version, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--version", &version, &argc, &argv, &error)) continue;
    if (!module && FbleParseSearchPathArg(&search_path, &argc, &argv, &error)) continue;
    if (!module && FbleParseStringArg("-m", &module_path, &argc, &argv, &error)) continue;
    if (!module && FbleParseStringArg("--module", &module_path, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--profile", &profile_file, &argc, &argv, &error)) continue;

    end_of_options = true;
    if (strcmp(argv[0], "--") == 0) {
      argc--;
      argv++;
    }
  }

  if (version) {
    printf("fble-stdio %s\n", FBLE_VERSION);
    FbleVectorFree(search_path);
    return EX_TRUE;
  }

  if (help) {
    PrintUsage(stdout, module);
    FbleVectorFree(search_path);
    return EX_TRUE;
  }

  if (error) {
    PrintUsage(stderr, module);
    FbleVectorFree(search_path);
    return EX_USAGE;
  }

  if (!module && module_path == NULL) {
    fprintf(stderr, "missing required --module option.\n");
    PrintUsage(stderr, module);
    FbleVectorFree(search_path);
    return EX_USAGE;
  }

  FILE* fprofile = NULL;
  if (profile_file != NULL) {
    fprofile = fopen(profile_file, "w");
    if (fprofile == NULL) {
      fprintf(stderr, "unable to open %s for writing.\n", profile_file);
      FbleVectorFree(search_path);
      return EX_FAILURE;
    }
  }

  FbleProfile* profile = fprofile == NULL ? NULL : FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();

  FbleValue* stdio = FbleLinkFromCompiledOrSource(heap, profile, module, search_path, module_path);
  FbleVectorFree(search_path);
  if (stdio == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAILURE;
  }

  FbleValueV stdio_args;
  FbleVectorInit(stdio_args);
  for (int i = 0; i < argc; ++i) {
    FbleVectorAppend(stdio_args, FbleNewStringValue(heap, argv[i]));
  }

  FbleValue* value = FbleStdio(heap, profile, stdio, stdio_args.size, stdio_args.xs);

  FbleReleaseValue(heap, stdio);
  for (size_t i = 0; i < stdio_args.size; ++i) {
    FbleReleaseValue(heap, stdio_args.xs[i]);
  }
  FbleVectorFree(stdio_args);

  size_t result = EX_FAILURE;
  if (value != NULL) {
    result = FbleUnionValueTag(value);
    FbleReleaseValue(heap, value);
  }

  FbleFreeValueHeap(heap);

  if (fprofile != NULL) {
    FbleProfileReport(fprofile, profile);
  }

  FbleFreeProfile(profile);
  return result;
}
