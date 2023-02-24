
// stdio.fble.c --
//   Implementation of FbleStdio and FbleStdioMain functions.

#include "stdio.fble.h"

#include <stdio.h>      // for FILE, fprintf, fflush, fgetc
#include <stdlib.h>     // for free
#include <string.h>     // for strcmp

#include <fble/fble-alloc.h>       // for FbleFree
#include <fble/fble-arg-parse.h>   // for FbleParseBoolArg, etc.
#include <fble/fble-link.h>        // for FbleCompiledModuleFunction.
#include <fble/fble-value.h>       // for FbleValue, etc.
#include <fble/fble-vector.h>      // for FbleVectorInit.
#include <fble/fble-version.h>     // for FBLE_VERSION

#include "char.fble.h"        // for FbleCharValueAccess
#include "int.fble.h"         // for FbleNewIntValue, FbleIntValueAccess
#include "string.fble.h"      // for FbleNewStringValue, FbleStringValueAccess

#define EX_TRUE 0
#define EX_FALSE 1
#define EX_USAGE 2
#define EX_FAILURE 3

extern const char* BUILDSTAMP;

/**
 * An FbleExecutable with a FILE handle.
 */
typedef struct {
  FbleExecutable _base;
  FILE* file;
} FileExecutable;

static FbleValue* IStreamImpl(
    FbleValueHeap* heap, FbleThread* thread,
    FbleExecutable* executable,
    FbleValue** args, FbleValue** statics,
    FbleBlockId profile_block_offset);
static FbleValue* OStreamImpl(
    FbleValueHeap* heap, FbleThread* thread,
    FbleExecutable* executable,
    FbleValue** args, FbleValue** statics,
    FbleBlockId profile_block_offset);
static FbleValue* IStream(FbleValueHeap* heap, FILE* file, FbleBlockId proble_block_offset);
static FbleValue* OStream(FbleValueHeap* heap, FILE* file, FbleBlockId proble_block_offset);

static void PrintUsage(FILE* stream, FbleCompiledModuleFunction* module);

// IStream -- Read a byte from a file.
//   IO@<Maybe@<Int@>>
static FbleValue* IStreamImpl(
    FbleValueHeap* heap, FbleThread* thread,
    FbleExecutable* executable,
    FbleValue** args, FbleValue** statics,
    FbleBlockId profile_block_offset)
{
  (void)thread;
  (void)statics;
  (void)profile_block_offset;

  FileExecutable* file = (FileExecutable*)executable;

  FbleValue* world = args[0];

  int c = fgetc(file->file);
  FbleValue* ms;
  if (c == EOF) {
    ms = FbleNewEnumValue(heap, 1);
  } else {
    FbleValue* v = FbleNewIntValue(heap, c);
    ms = FbleNewUnionValue(heap, 0, v);
    FbleReleaseValue(heap, v);
  }

  FbleValue* result = FbleNewStructValue_(heap, 2, world, ms);
  FbleReleaseValue(heap, ms);
  return result;
}

// OStream -- Write a byte to a file.
//   (Int@, World@) { R@<Unit@>; }
static FbleValue* OStreamImpl(
    FbleValueHeap* heap, FbleThread* thread,
    FbleExecutable* executable,
    FbleValue** args, FbleValue** statics,
    FbleBlockId profile_block_offset)
{
  (void)thread;
  (void)statics;
  (void)profile_block_offset;

  FileExecutable* file = (FileExecutable*)executable;

  FbleValue* byte = args[0];
  FbleValue* world = args[1];

  int64_t c = FbleIntValueAccess(byte);
  fputc(c, file->file);
  fflush(file->file);

  FbleValue* unit = FbleNewStructValue_(heap, 0);
  FbleValue* result = FbleNewStructValue_(heap, 2, world, unit);
  FbleReleaseValue(heap, unit);
  return result;
}

static FbleValue* IStream(FbleValueHeap* heap, FILE* file, FbleBlockId profile_block_offset)
{
  FileExecutable* exe = FbleAlloc(FileExecutable);
  exe->_base.refcount = 0;
  exe->_base.magic = FBLE_EXECUTABLE_MAGIC;
  exe->_base.num_args = 1;
  exe->_base.num_statics = 0;
  exe->_base.profile_block_id = 0;
  exe->_base.run = &IStreamImpl;
  exe->_base.on_free = &FbleExecutableNothingOnFree;
  exe->file = file;
  return FbleNewFuncValue(heap, &exe->_base, profile_block_offset, NULL);
}

static FbleValue* OStream(FbleValueHeap* heap, FILE* file, FbleBlockId profile_block_offset)
{
  FileExecutable* exe = FbleAlloc(FileExecutable);
  exe->_base.refcount = 0;
  exe->_base.magic = FBLE_EXECUTABLE_MAGIC;
  exe->_base.num_args = 2;
  exe->_base.num_statics = 0;
  exe->_base.profile_block_id = 1;
  exe->_base.run = &OStreamImpl;
  exe->_base.on_free = &FbleExecutableNothingOnFree;
  exe->file = file;
  return FbleNewFuncValue(heap, &exe->_base, profile_block_offset, NULL);
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
  block_names[0].name = FbleNewString("istream");
  block_names[0].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  block_names[1].name = FbleNewString("ostream");
  block_names[1].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  FbleBlockId block_id = 0;
  if (profile != NULL) {
    FbleNameV names = { .size = 2, .xs = block_names };
    block_id = FbleProfileAddBlocks(profile, names);
  };
  FbleFreeName(block_names[0]);
  FbleFreeName(block_names[1]);

  FbleValue* fble_stdin = IStream(heap, stdin, block_id);
  FbleValue* fble_stdout = OStream(heap, stdout, block_id);
  FbleValue* fble_stderr = OStream(heap, stderr, block_id);

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

  FbleSearchPath* search_path = FbleNewSearchPath();
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
    if (!module && FbleParseSearchPathArg(search_path, &argc, &argv, &error)) continue;
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
    printf("fble-stdio %s (%s)\n", FBLE_VERSION, FbleBuildStamp);
    FbleFreeSearchPath(search_path);
    return EX_TRUE;
  }

  if (help) {
    PrintUsage(stdout, module);
    FbleFreeSearchPath(search_path);
    return EX_TRUE;
  }

  if (error) {
    PrintUsage(stderr, module);
    FbleFreeSearchPath(search_path);
    return EX_USAGE;
  }

  if (!module && module_path == NULL) {
    fprintf(stderr, "missing required --module option.\n");
    PrintUsage(stderr, module);
    FbleFreeSearchPath(search_path);
    return EX_USAGE;
  }

  FILE* fprofile = NULL;
  if (profile_file != NULL) {
    fprofile = fopen(profile_file, "w");
    if (fprofile == NULL) {
      fprintf(stderr, "unable to open %s for writing.\n", profile_file);
      FbleFreeSearchPath(search_path);
      return EX_FAILURE;
    }
  }

  FbleProfile* profile = fprofile == NULL ? NULL : FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();

  FbleValue* stdio = FbleLinkFromCompiledOrSource(heap, profile, module, search_path, module_path);
  FbleFreeSearchPath(search_path);
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
  FbleFreeVector(stdio_args);

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
