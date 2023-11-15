
// stdio.fble.c --
//   Implementation of FbleStdio and FbleStdioMain functions.

#include "stdio.fble.h"

#include <stdio.h>      // for FILE, fprintf, fflush, fgetc
#include <stdlib.h>     // for free
#include <string.h>     // for strcmp

#include <fble/fble-alloc.h>       // for FbleFree
#include <fble/fble-arg-parse.h>   // for FbleParseBoolArg, etc.
#include <fble/fble-link.h>        // for FbleCompiledModuleFunction.
#include <fble/fble-usage.h>       // for FblePrintUsageDoc
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

/**
 * An FbleExecutable with a FILE handle.
 */
typedef struct {
  FbleExecutable _base;
  FILE* file;
} FileExecutable;

static void OnFree(FbleExecutable* this);
static FbleValue* IStreamImpl(
    FbleValueHeap* heap, FbleValue** tail_call_buffer,
    FbleExecutable* executable,
    FbleValue** args, FbleValue** statics,
    FbleBlockId profile_block_offset,
    FbleProfileThread* profile);
static FbleValue* OStreamImpl(
    FbleValueHeap* heap, FbleValue** tail_call_buffer,
    FbleExecutable* executable,
    FbleValue** args, FbleValue** statics,
    FbleBlockId profile_block_offset,
    FbleProfileThread* profile);
static FbleValue* ReadImpl(
    FbleValueHeap* heap, FbleValue** tail_call_buffer,
    FbleExecutable* executable,
    FbleValue** args, FbleValue** statics,
    FbleBlockId profile_block_offset,
    FbleProfileThread* profile);
static FbleValue* IStream(FbleValueHeap* heap, FILE* file, FbleBlockId profile_block_offset);
static FbleValue* OStream(FbleValueHeap* heap, FILE* file, FbleBlockId profile_block_offset);
static FbleValue* Read(FbleValueHeap* heap, FbleBlockId profile_block_offset);


// OnFree function for FileExecutable
static void OnFree(FbleExecutable* this)
{
  FileExecutable* file = (FileExecutable*)this;

  // Don't close stderr, because that could prevent us from seeing runtime
  // errors printed to stderr.
  if (file->file != stderr) {
    fclose(file->file);
  }
}

// IStream -- Read a byte from a file.
//   IO@<Maybe@<Int@>>
static FbleValue* IStreamImpl(
    FbleValueHeap* heap, FbleValue** tail_call_buffer,
    FbleExecutable* executable,
    FbleValue** args, FbleValue** statics,
    FbleBlockId profile_block_offset,
    FbleProfileThread* profile)
{
  (void)tail_call_buffer;
  (void)statics;
  (void)profile_block_offset;
  (void)profile;

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
    FbleValueHeap* heap, FbleValue** tail_call_buffer,
    FbleExecutable* executable,
    FbleValue** args, FbleValue** statics,
    FbleBlockId profile_block_offset,
    FbleProfileThread* profile)
{
  (void)tail_call_buffer;
  (void)statics;
  (void)profile_block_offset;
  (void)profile;

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

// Read -- Open a file for reading.
//   (String@, World@) { R@<Maybe@<IStream@<R@>>>; }
static FbleValue* ReadImpl(
    FbleValueHeap* heap, FbleValue** tail_call_buffer,
    FbleExecutable* executable,
    FbleValue** args, FbleValue** statics,
    FbleBlockId profile_block_offset,
    FbleProfileThread* profile)
{
  (void)tail_call_buffer;
  (void)executable;
  (void)statics;
  (void)profile_block_offset;
  (void)profile;

  char* filename = FbleStringValueAccess(args[0]);
  FbleValue* world = args[1];
  FILE* fin = fopen(filename, "r");
  FbleFree(filename);

  FbleValue* mstream;
  if (fin == NULL) {
    mstream = FbleNewEnumValue(heap, 1); // Nothing
  } else {
    FbleValue* stream = IStream(heap, fin, profile_block_offset);
    mstream = FbleNewUnionValue(heap, 0, stream); // Just(stream)
    FbleReleaseValue(heap, stream);
  }

  FbleValue* result = FbleNewStructValue_(heap, 2, world, mstream);
  FbleReleaseValue(heap, mstream);
  return result;
}

static FbleValue* IStream(FbleValueHeap* heap, FILE* file, FbleBlockId profile_block_offset)
{
  FileExecutable* exe = FbleAlloc(FileExecutable);
  exe->_base.refcount = 0;
  exe->_base.magic = FBLE_EXECUTABLE_MAGIC;
  exe->_base.num_args = 1;
  exe->_base.num_statics = 0;
  exe->_base.tail_call_buffer_size = 0;
  exe->_base.profile_block_id = 0;
  exe->_base.run = &IStreamImpl;
  exe->_base.on_free = &OnFree;
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
  exe->_base.tail_call_buffer_size = 0;
  exe->_base.profile_block_id = 1;
  exe->_base.run = &OStreamImpl;
  exe->_base.on_free = &OnFree;
  exe->file = file;
  return FbleNewFuncValue(heap, &exe->_base, profile_block_offset, NULL);
}

static FbleValue* Read(FbleValueHeap* heap, FbleBlockId profile_block_offset)
{
  FbleExecutable* exe = FbleAlloc(FbleExecutable);
  exe->refcount = 0;
  exe->magic = FBLE_EXECUTABLE_MAGIC;
  exe->num_args = 2;
  exe->num_statics = 0;
  exe->tail_call_buffer_size = 0;
  exe->profile_block_id = 2;
  exe->run = &ReadImpl;
  exe->on_free = &FbleExecutableNothingOnFree;
  return FbleNewFuncValue(heap, exe, profile_block_offset, NULL);
}

// FbleStdio -- see documentation in stdio.fble.h
FbleValue* FbleStdio(FbleValueHeap* heap, FbleProfile* profile, FbleValue* stdio, size_t argc, FbleValue** argv)
{
  FbleValue* func = FbleEval(heap, stdio, profile);
  if (func == NULL) {
    return NULL;
  }

  FbleName block_names[3];
  block_names[0].name = FbleNewString("istream");
  block_names[0].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  block_names[1].name = FbleNewString("ostream");
  block_names[1].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  block_names[2].name = FbleNewString("read");
  block_names[2].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  FbleBlockId block_id = 0;
  if (profile != NULL) {
    FbleNameV names = { .size = 3, .xs = block_names };
    block_id = FbleProfileAddBlocks(profile, names);
  };
  FbleFreeName(block_names[0]);
  FbleFreeName(block_names[1]);
  FbleFreeName(block_names[2]);

  FbleValue* fble_stdin = IStream(heap, stdin, block_id);
  FbleValue* fble_stdout = OStream(heap, stdout, block_id);
  FbleValue* fble_stderr = OStream(heap, stderr, block_id);
  FbleValue* fble_read = Read(heap, block_id);

  FbleValue* argS = FbleNewEnumValue(heap, 1);
  for (size_t i = 0; i < argc; ++i) {
    FbleValue* argP = FbleNewStructValue_(heap, 2, argv[argc - i -1], argS);
    FbleReleaseValue(heap, argS);
    argS = FbleNewUnionValue(heap, 0, argP);
    FbleReleaseValue(heap, argP);
  }

  FbleValue* args[5] = { fble_stdin, fble_stdout, fble_stderr, fble_read, argS };
  FbleValue* computation = FbleApply(heap, func, args, profile);
  FbleReleaseValue(heap, func);
  FbleReleaseValue(heap, fble_stdin);
  FbleReleaseValue(heap, fble_stdout);
  FbleReleaseValue(heap, fble_stderr);
  FbleReleaseValue(heap, fble_read);
  FbleReleaseValue(heap, argS);

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
  const char* arg0 = argv[0];

  // To ease debugging of FbleStdioMain programs, cause the following useful
  // functions to be linked in:
  (void)(FbleCharValueAccess);
  (void)(FbleIntValueAccess);
  (void)(FbleStringValueAccess);

  FbleModuleArg module_arg = FbleNewModuleArg();
  const char* profile_file = NULL;
  bool end_of_options = false;
  bool help = false;
  bool error = false;
  bool version = false;

  // If the module is compiled and '--' isn't present, skip to end of options
  // right away. That way precompiled programs can go straight to application
  // args if they want.
  if (module != NULL) {
    end_of_options = true;
    for (int i = 0; i < argc; ++i) {
      if (strcmp(argv[i], "--") == 0) {
        end_of_options = false;
        break;
      }
    }
  }

  argc--;
  argv++;
  while (!(help || error || version) && !end_of_options && argc > 0) {
    if (FbleParseBoolArg("-h", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--help", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("-v", &version, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--version", &version, &argc, &argv, &error)) continue;
    if (!module && FbleParseModuleArg(&module_arg, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--profile", &profile_file, &argc, &argv, &error)) continue;

    end_of_options = true;
    if (strcmp(argv[0], "--") == 0) {
      argc--;
      argv++;
    }
  }

  if (version) {
    FblePrintCompiledHeaderLine(stdout, "fble-stdio", arg0, module);
    FblePrintVersion(stdout, "fble-stdio");
    FbleFreeModuleArg(module_arg);
    return EX_TRUE;
  }

  if (help) {
    FblePrintCompiledHeaderLine(stdout, "fble-stdio", arg0, module);
    FblePrintUsageDoc(arg0, "fble-stdio.usage.txt");
    FbleFreeModuleArg(module_arg);
    return EX_TRUE;
  }

  if (error) {
    fprintf(stderr, "Try --help for usage info.\n");
    FbleFreeModuleArg(module_arg);
    return EX_USAGE;
  }

  if (!module && module_arg.module_path == NULL) {
    fprintf(stderr, "missing required --module option.\n");
    fprintf(stderr, "Try --help for usage info.\n");
    FbleFreeModuleArg(module_arg);
    return EX_USAGE;
  }

  FILE* fprofile = NULL;
  if (profile_file != NULL) {
    fprofile = fopen(profile_file, "w");
    if (fprofile == NULL) {
      fprintf(stderr, "unable to open %s for writing.\n", profile_file);
      FbleFreeModuleArg(module_arg);
      return EX_FAILURE;
    }
  }

  FbleProfile* profile = fprofile == NULL ? NULL : FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();

  FbleValue* stdio = FbleLinkFromCompiledOrSource(heap, profile, module, module_arg.search_path, module_arg.module_path);
  FbleFreeModuleArg(module_arg);
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
