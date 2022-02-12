
// stdio.fble.c --
//   Implementation of FbleStdio and FbleStdioMain functions.


#define _GNU_SOURCE     // for getline

#include "stdio.fble.h"

#include <stdio.h>      // for FILE, fprintf, fflush, getline
#include <stdlib.h>     // for free
#include <string.h>     // for strcmp

#include "fble-alloc.h"       // for FbleFree
#include "fble-arg-parse.h"   // for FbleParseBoolArg, etc.
#include "fble-link.h"        // for FbleCompiledModuleFunction.
#include "fble-value.h"       // for FbleValue, etc.
#include "fble-vector.h"      // for FbleVectorInit.

#include "Core/char.fble.h"    // for FbleCharValueAccess
#include "Core/int.fble.h"     // for FbleIntValueAccess
#include "Core/string.fble.h"  // for FbleNewStringValue, FbleStringValueAccess

#define EX_TRUE 0
#define EX_FALSE 1
#define EX_USAGE 2
#define EX_FAILURE 3

typedef struct {
  FbleIO io;

  FbleValue* in;
  FbleValue* out;
  FbleValue* err;
} Stdio;

static void Output(FILE* stream, FbleValue* str);
static bool IO(FbleIO* io, FbleValueHeap* heap, bool block);
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
// IO --
//   FbleIO.io function for external ports.
//   See the corresponding documentation in fble-value.h.
//
// Ports:
//  in:   Read a line from stdin. Nothing on end of file.
//  out:  Write to stdout.
//  err:  Write to stderr.
static bool IO(FbleIO* io, FbleValueHeap* heap, bool block)
{
  Stdio* stdio = (Stdio*)io;
  bool change = false;
  if (stdio->out != NULL) {
    Output(stdout, stdio->out);
    FbleReleaseValue(heap, stdio->out);
    stdio->out = NULL;
    change = true;
  }

  if (stdio->err != NULL) {
    Output(stderr, stdio->err);
    FbleReleaseValue(heap, stdio->err);
    stdio->err = NULL;
    change = true;
  }

  if (block && stdio->in == NULL) {
    // Read a line from stdin.
    char* line = NULL;
    size_t len = 0;
    ssize_t read = getline(&line, &len, stdin);
    if (read < 0) {
      stdio->in = FbleNewEnumValue(heap, 1);
    } else {
      FbleValue* charS = FbleNewStringValue(heap, line);
      stdio->in = FbleNewUnionValue(heap, 0, charS);
      FbleReleaseValue(heap, charS);
    }
    free(line);
    change = true;
  }
  return change;
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
      "     Print this help message and exit.\n");
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

  Stdio io = {
    .io = { .io = &IO, },
    .in = NULL,
    .out = NULL,
    .err = NULL,
  };

  FbleName block_names[5];
  block_names[0].name = FbleNewString("stdin!");
  block_names[0].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  block_names[1].name = FbleNewString("stdout!");
  block_names[1].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  block_names[2].name = FbleNewString("stdout!!");
  block_names[2].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  block_names[3].name = FbleNewString("stderr!");
  block_names[3].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  block_names[4].name = FbleNewString("stderr!!");
  block_names[4].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  FbleBlockId block_id = 0;
  if (profile != NULL) {
    FbleNameV names = { .size = 5, .xs = block_names };
    block_id = FbleProfileAddBlocks(profile, names);
  };
  FbleFreeName(block_names[0]);
  FbleFreeName(block_names[1]);
  FbleFreeName(block_names[2]);
  FbleFreeName(block_names[3]);
  FbleFreeName(block_names[4]);

  FbleValue* fble_stdin = FbleNewInputPortValue(heap, &io.in, block_id);
  FbleValue* fble_stdout = FbleNewOutputPortValue(heap, &io.out, block_id + 1);
  FbleValue* fble_stderr = FbleNewOutputPortValue(heap, &io.err, block_id + 3);
  FbleValue* fble_io = FbleNewStructValue(heap, 3, fble_stdin, fble_stdout, fble_stderr);
  FbleReleaseValue(heap, fble_stdin);
  FbleReleaseValue(heap, fble_stdout);
  FbleReleaseValue(heap, fble_stderr);

  FbleValue* argS = FbleNewEnumValue(heap, 1);
  for (size_t i = 0; i < argc; ++i) {
    FbleValue* argP = FbleNewStructValue(heap, 2, argv[argc - i -1], argS);
    FbleReleaseValue(heap, argS);
    argS = FbleNewUnionValue(heap, 0, argP);
    FbleReleaseValue(heap, argP);
  }

  FbleValue* args[2] = { fble_io, argS };
  FbleValue* proc = FbleApply(heap, func, args, profile);
  FbleReleaseValue(heap, func);
  FbleReleaseValue(heap, args[0]);
  FbleReleaseValue(heap, args[1]);

  if (proc == NULL) {
    return NULL;
  }

  FbleValue* value = FbleExec(heap, &io.io, proc, profile);
  FbleReleaseValue(heap, proc);
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

  argc--;
  argv++;
  while (!error && !end_of_options && argc > 0) {
    if (FbleParseBoolArg("-h", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--help", &help, &argc, &argv, &error)) continue;
    if (!module && FbleParseSearchPathArg("-I", &search_path, &argc, &argv, &error)) continue;
    if (!module && FbleParseStringArg("-m", &module_path, &argc, &argv, &error)) continue;
    if (!module && FbleParseStringArg("--module", &module_path, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--profile", &profile_file, &argc, &argv, &error)) continue;

    end_of_options = true;
    if (strcmp(argv[0], "--") == 0) {
      argc--;
      argv++;
    }
  }

  if (help) {
    PrintUsage(stdout, module);
    FbleFree(search_path.xs);
    return EX_TRUE;
  }

  if (error) {
    PrintUsage(stderr, module);
    FbleFree(search_path.xs);
    return EX_USAGE;
  }

  if (!module && module_path == NULL) {
    fprintf(stderr, "missing required --module option.\n");
    PrintUsage(stderr, module);
    FbleFree(search_path.xs);
    return EX_USAGE;
  }

  FILE* fprofile = NULL;
  if (profile_file != NULL) {
    fprofile = fopen(profile_file, "w");
    if (fprofile == NULL) {
      fprintf(stderr, "unable to open %s for writing.\n", profile_file);
      FbleFree(search_path.xs);
      return EX_FAILURE;
    }
  }

  FbleProfile* profile = fprofile == NULL ? NULL : FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();

  FbleValue* stdio = FbleLinkFromCompiledOrSource(heap, profile, module, search_path, module_path);
  FbleFree(search_path.xs);
  if (stdio == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAILURE;
  }

  FbleValueV stdio_args;
  FbleVectorInit(stdio_args);
  for (size_t i = 0; i < argc; ++i) {
    FbleVectorAppend(stdio_args, FbleNewStringValue(heap, argv[i]));
  }

  FbleValue* value = FbleStdio(heap, profile, stdio, stdio_args.size, stdio_args.xs);

  FbleReleaseValue(heap, stdio);
  for (size_t i = 0; i < stdio_args.size; ++i) {
    FbleReleaseValue(heap, stdio_args.xs[i]);
  }
  FbleFree(stdio_args.xs);

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
