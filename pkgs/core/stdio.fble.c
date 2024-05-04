/**
 * @file stdio.fble.c
 *  Implementation of FbleStdio and FbleStdioMain functions.
 */

#include "stdio.fble.h"

#include <stdio.h>      // for FILE, fprintf, fflush, fgetc
#include <stdlib.h>     // for free
#include <string.h>     // for strcmp

#include <fble/fble-alloc.h>       // for FbleFree
#include <fble/fble-arg-parse.h>   // for FbleParseBoolArg, etc.
#include <fble/fble-generate.h>    // for FbleGeneratedModule
#include <fble/fble-link.h>        // for FbleLink
#include <fble/fble-usage.h>       // for FblePrintUsageDoc
#include <fble/fble-value.h>       // for FbleValue, etc.
#include <fble/fble-vector.h>      // for FbleInitVector.
#include <fble/fble-version.h>     // for FBLE_VERSION

#include "char.fble.h"        // for FbleCharValueAccess
#include "int.fble.h"         // for FbleNewIntValue, FbleIntValueAccess
#include "string.fble.h"      // for FbleNewStringValue, FbleStringValueAccess

#define EX_TRUE 0
#define EX_FALSE 1
#define EX_USAGE 2
#define EX_FAILURE 3

static void OnFree(void* data);
static FbleValue* IStreamImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);
static FbleValue* OStreamImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);
static FbleValue* ReadImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);
static FbleValue* WriteImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);
static FbleValue* GetEnvImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);
static FbleValue* IStream(FbleValueHeap* heap, FILE* file, FbleBlockId profile_block_id);
static FbleValue* OStream(FbleValueHeap* heap, FILE* file, FbleBlockId profile_block_id);
static FbleValue* Read(FbleValueHeap* heap, FbleBlockId profile_block_id);
static FbleValue* Write(FbleValueHeap* heap, FbleBlockId profile_block_id);
static FbleValue* GetEnv(FbleValueHeap* heap, FbleBlockId profile_block_id);


/**
 * @func[OnFree] OnFree function for FILE* native values.
 *  See documentation of FbleNewNativeValue in fble-value.h.
 */
static void OnFree(void* data)
{
  FILE* file = (FILE*)data;

  // Don't close stderr, because that could prevent us from seeing runtime
  // errors printed to stderr.
  if (file != stderr) {
    fclose(file);
  }
}

// IStreamImpl -- Read a byte from a file.
//   IO@<Maybe@<Int@>>
static FbleValue* IStreamImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;

  FILE* file = (FILE*)FbleNativeValueData(function->statics[0]);

  FbleValue* world = args[0];

  int c = fgetc(file);
  FbleValue* ms;
  if (c == EOF) {
    ms = FbleNewEnumValue(heap, 1);
  } else {
    FbleValue* v = FbleNewIntValue(heap, c);
    ms = FbleNewUnionValue(heap, 0, v);
  }

  return FbleNewStructValue_(heap, 2, world, ms);
}

// OStreamImpl -- Write a byte to a file.
//   (Int@, World@) { R@<Unit@>; }
static FbleValue* OStreamImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;

  FILE* file = (FILE*)FbleNativeValueData(function->statics[0]);

  FbleValue* byte = args[0];
  FbleValue* world = args[1];

  int64_t c = FbleIntValueAccess(byte);
  fputc(c, file);
  fflush(file);

  FbleValue* unit = FbleNewStructValue_(heap, 0);
  return FbleNewStructValue_(heap, 2, world, unit);
}

// ReadImpl -- Open a file for reading.
//   (String@, World@) { R@<Maybe@<IStream@<R@>>>; }
static FbleValue* ReadImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;

  char* filename = FbleStringValueAccess(args[0]);
  FbleValue* world = args[1];
  FILE* fin = fopen(filename, "r");
  FbleFree(filename);

  FbleValue* mstream;
  if (fin == NULL) {
    mstream = FbleNewEnumValue(heap, 1); // Nothing
  } else {
    FbleValue* stream = IStream(heap, fin, function->profile_block_id - 2);
    mstream = FbleNewUnionValue(heap, 0, stream); // Just(stream)
  }

  return FbleNewStructValue_(heap, 2, world, mstream);
}

// WriteImpl -- Open a file for writing.
//   (String@, World@) { R@<Maybe@<OStream@<R@>>>; }
static FbleValue* WriteImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;

  char* filename = FbleStringValueAccess(args[0]);
  FbleValue* world = args[1];
  FILE* fin = fopen(filename, "w");
  FbleFree(filename);

  FbleValue* mstream;
  if (fin == NULL) {
    mstream = FbleNewEnumValue(heap, 1); // Nothing
  } else {
    FbleValue* stream = OStream(heap, fin, function->profile_block_id - 2);
    mstream = FbleNewUnionValue(heap, 0, stream); // Just(stream)
  }

  return FbleNewStructValue_(heap, 2, world, mstream);
}

// GetEnvImpl -- Get an environment variable.
//   (String@, World@) { R@<Maybe@<String@>>; }
static FbleValue* GetEnvImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;

  char* var = FbleStringValueAccess(args[0]);
  FbleValue* world = args[1];
  char* value = getenv(var);
  FbleFree(var);

  FbleValue* mstr;
  if (value == NULL) {
    mstr = FbleNewEnumValue(heap, 1); // Nothing
  } else {
    FbleValue* str = FbleNewStringValue(heap, value);
    mstr = FbleNewUnionValue(heap, 0, str); // Just(str)
  }

  return FbleNewStructValue_(heap, 2, world, mstr);
}

// IStream --
//  Creates an IStream@ value for the given file.
static FbleValue* IStream(FbleValueHeap* heap, FILE* file, FbleBlockId profile_block_id)
{
  FbleValue* native = FbleNewNativeValue(heap, file, &OnFree);

  FbleExecutable exe = {
    .num_args = 1,
    .num_statics = 1,
    .run = &IStreamImpl,
  };

  return FbleNewFuncValue(heap, &exe, profile_block_id, &native);
}

// OStream --
//  Creates an OStream@ value for the given file.
static FbleValue* OStream(FbleValueHeap* heap, FILE* file, FbleBlockId profile_block_id)
{
  FbleValue* native = FbleNewNativeValue(heap, file, &OnFree);

  FbleExecutable exe = {
    .num_args = 2,
    .num_statics = 1,
    .run = &OStreamImpl,
  };

  return FbleNewFuncValue(heap, &exe, profile_block_id, &native);
}

static FbleValue* Read(FbleValueHeap* heap, FbleBlockId profile_block_id)
{
  FbleExecutable exe = {
    .num_args = 2,
    .num_statics = 0,
    .run = &ReadImpl,
  };
  return FbleNewFuncValue(heap, &exe, profile_block_id, NULL);
}

static FbleValue* Write(FbleValueHeap* heap, FbleBlockId profile_block_id)
{
  FbleExecutable exe = {
    .num_args = 2,
    .num_statics = 0,
    .run = &WriteImpl,
  };
  return FbleNewFuncValue(heap, &exe, profile_block_id, NULL);
}

static FbleValue* GetEnv(FbleValueHeap* heap, FbleBlockId profile_block_id)
{
  FbleExecutable exe = {
    .num_args = 2,
    .num_statics = 0,
    .run = &GetEnvImpl,
  };
  return FbleNewFuncValue(heap, &exe, profile_block_id, NULL);
}

// FbleNewStdioIO -- see documentation in stdio.fble.h
FbleValue* FbleNewStdioIO(FbleValueHeap* heap, FbleProfile* profile)
{
  FbleName block_names[5];
  block_names[0].name = FbleNewString("istream");
  block_names[0].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  block_names[1].name = FbleNewString("ostream");
  block_names[1].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  block_names[2].name = FbleNewString("read");
  block_names[2].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  block_names[3].name = FbleNewString("write");
  block_names[3].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  block_names[4].name = FbleNewString("getenv");
  block_names[4].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  FbleNameV names = { .size = 5, .xs = block_names };
  FbleBlockId block_id = FbleAddBlocksToProfile(profile, names);
  for (size_t i = 0; i < 5; ++i) {
    FbleFreeName(block_names[i]);
  }

  FbleValue* fble_stdin = IStream(heap, stdin, block_id);
  FbleValue* fble_stdout = OStream(heap, stdout, block_id + 1);
  FbleValue* fble_stderr = OStream(heap, stderr, block_id + 1);
  FbleValue* fble_read = Read(heap, block_id + 2);
  FbleValue* fble_write = Write(heap, block_id + 3);
  FbleValue* fble_getenv = GetEnv(heap, block_id + 4);
  FbleValue* fble_stdio = FbleNewStructValue_(heap, 6,
      fble_stdin, fble_stdout, fble_stderr,
      fble_read, fble_write, fble_getenv);
  return fble_stdio;
}

// FbleStdio -- see documentation in stdio.fble.h
FbleValue* FbleStdio(FbleValueHeap* heap, FbleProfile* profile, FbleValue* stdio, size_t argc, FbleValue** argv)
{
  FbleValue* func = FbleEval(heap, stdio, profile);
  if (func == NULL) {
    return NULL;
  }

  FblePushFrame(heap);
  FbleValue* fble_stdio = FbleNewStdioIO(heap, profile);
  FbleValue* argS = FbleNewEnumValue(heap, 1);
  for (size_t i = 0; i < argc; ++i) {
    FbleValue* argP = FbleNewStructValue_(heap, 2, argv[argc - i -1], argS);
    argS = FbleNewUnionValue(heap, 0, argP);
  }

  FbleValue* args[2] = { fble_stdio, argS };
  FbleValue* computation = FbleApply(heap, func, 2, args, profile);

  if (computation == NULL) {
    return FblePopFrame(heap, NULL);
  }

  // computation has type IO@<Bool@>, which is (World@) { R@<Bool@>; }
  FbleValue* world = FbleNewStructValue_(heap, 0);
  FbleValue* result = FbleApply(heap, computation, 1, &world, profile);
  if (result == NULL) {
    return FblePopFrame(heap, NULL);
  }

  // result has type R@<Bool@>, which is *(s, x)
  FbleValue* value = FbleStructValueField(result, 1);
  return FblePopFrame(heap, value);
}

// FbleStdioMain -- See documentation in stdio.fble.h
int FbleStdioMain(int argc, const char** argv, FbleGeneratedModule* module)
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

  FbleProfile* profile = FbleNewProfile(fprofile != NULL);
  FbleValueHeap* heap = FbleNewValueHeap();

  FbleValue* stdio = FbleLink(heap, profile, module, module_arg.search_path, module_arg.module_path);
  FbleFreeModuleArg(module_arg);
  if (stdio == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAILURE;
  }

  FbleValueV stdio_args;
  FbleInitVector(stdio_args);
  for (int i = 0; i < argc; ++i) {
    FbleAppendToVector(stdio_args, FbleNewStringValue(heap, argv[i]));
  }

  FbleValue* value = FbleStdio(heap, profile, stdio, stdio_args.size, stdio_args.xs);

  FbleFreeVector(stdio_args);

  size_t result = EX_FAILURE;
  if (value != NULL) {
    result = FbleUnionValueTag(value);
  }

  FbleFreeValueHeap(heap);

  FbleGenerateProfileReport(fprofile, profile);
  FbleFreeProfile(profile);
  return result;
}
