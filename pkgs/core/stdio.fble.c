/**
 * @file stdio.fble.c
 *  Implementation of FbleStdio and FbleStdioMain functions.
 */

#include "stdio.fble.h"

#include <stdio.h>      // for FILE, fprintf, fflush, fgetc
#include <stdlib.h>     // for free
#include <string.h>     // for strcmp

#include <fble/fble-alloc.h>       // for FbleFree
#include <fble/fble-main.h>        // for FbleMain.
#include <fble/fble-program.h>     // for FblePreloadedModule
#include <fble/fble-value.h>       // for FbleValue, etc.
#include <fble/fble-vector.h>      // for FbleInitVector.

#include "fble-stdio.usage.h"      // for fbldUsageHelpText

#include "char.fble.h"        // for FbleCharValueAccess
#include "int.fble.h"         // for FbleNewIntValue, FbleIntValueAccess
#include "string.fble.h"      // for FbleNewStringValue, FbleStringValueAccess

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
 *  @arg[void*][data] FILE* pointer to close.
 *  @sideeffects
 *   Closes the @a[data] file.
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

/**
 * @func[IStreamImpl] FbleRunFunction to read a byte from a file.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is @l{IO@<Maybe@<Int@>>}.
 *
 *  @sideeffects
 *   Reads a byte from the file.
 */
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

/**
 * @func[OStreamImpl] FbleRunFunction to write a byte to a file.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is @l{(Int@, World@) { R@<Unit@>; }}.
 */
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

/**
 * @func[ReadImpl] FbleRunFunction to open a file for reading.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (String@, World@) { R@<Maybe@<IStream@<R@>>>; }
 */
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

/**
 * @func[WriteImpl] FbleRunFunction to open a file for writing.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (String@, World@) { R@<Maybe@<OStream@<R@>>>; }
 */
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

/**
 * @func[GetEnvImpl] FbleRunFunction to read and environment variable.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (String@, World@) { R@<Maybe@<String@>>; }
 */
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

/**
 * @func[IStream] Allocates an @l{IStream@} for a file.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FILE*][file] The FILE to allocate the @l{IStream@} for.
 *  @arg[FbleBlockId][profile_block_id]
 *   The profile_block_id of the function to allocate.
 *
 *  @returns[FbleValue*] An fble @l{IStream@} function value.
 *
 *  @sideeffects
 *   Allocates a value on the heap.
 */
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

/**
 * @func[OStream] Allocates an @l{OStream@} for a file.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FILE*][file] The FILE to allocate the @l{OStream@} for.
 *  @arg[FbleBlockId][profile_block_id]
 *   The profile_block_id of the function to allocate.
 *
 *  @returns[FbleValue*] An fble @l{OStream@} function value.
 *
 *  @sideeffects
 *   Allocates a value on the heap.
 */
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

/**
 * @func[Read] Allocates an fble read function.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleBlockId][profile_block_id] The profile block id for the function.
 *  @returns[FbleValue*] The allocated function.
 *  @sideeffects Allocates an fble value.
 */
static FbleValue* Read(FbleValueHeap* heap, FbleBlockId profile_block_id)
{
  FbleExecutable exe = {
    .num_args = 2,
    .num_statics = 0,
    .run = &ReadImpl,
  };
  return FbleNewFuncValue(heap, &exe, profile_block_id, NULL);
}

/**
 * @func[Write] Allocates an fble write function.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleBlockId][profile_block_id] The profile block id for the function.
 *  @returns[FbleValue*] The allocated function.
 *  @sideeffects Allocates an fble value.
 */
static FbleValue* Write(FbleValueHeap* heap, FbleBlockId profile_block_id)
{
  FbleExecutable exe = {
    .num_args = 2,
    .num_statics = 0,
    .run = &WriteImpl,
  };
  return FbleNewFuncValue(heap, &exe, profile_block_id, NULL);
}

/**
 * @func[GetEnv] Allocates an fble getenv function.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleBlockId][profile_block_id] The profile block id for the function.
 *  @returns[FbleValue*] The allocated function.
 *  @sideeffects Allocates an fble value.
 */
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
int FbleStdioMain(int argc, const char** argv, FblePreloadedModule* preloaded)
{
  // To ease debugging of FbleStdioMain programs, cause the following useful
  // functions to be linked in:
  (void)(FbleCharValueAccess);
  (void)(FbleIntValueAccess);
  (void)(FbleStringValueAccess);

  // If the module is compiled and '--' isn't present, skip to end of options
  // right away. That way precompiled programs can go straight to application
  // args if they want.
  bool end_of_options = true;
  if (preloaded != NULL) {
    for (int i = 0; i < argc; ++i) {
      if (strcmp(argv[i], "--") == 0) {
        end_of_options = false;
        break;
      }
    }

    if (end_of_options) {
      argc = 1;
    }
  }

  FbleProfile* profile = FbleNewProfile(false);
  FbleValueHeap* heap = FbleNewValueHeap();
  FILE* profile_output_file = NULL;
  FbleValue* stdio = NULL;

  FbleMainStatus status = FbleMain(NULL, NULL, "fble-stdio", fbldUsageHelpText,
      &argc, &argv, preloaded, heap, profile, &profile_output_file, &stdio);

  if (stdio == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return status;
  }

  FbleValueV stdio_args;
  FbleInitVector(stdio_args);
  for (int i = 0; argv[i] != NULL; ++i) {
    FbleAppendToVector(stdio_args, FbleNewStringValue(heap, argv[i]));
  }

  FbleValue* value = FbleStdio(heap, profile, stdio, stdio_args.size, stdio_args.xs);

  FbleFreeVector(stdio_args);

  size_t result = FBLE_MAIN_OTHER_ERROR;
  if (value != NULL) {
    result = FbleUnionValueTag(value);
  }

  FbleFreeValueHeap(heap);

  FbleGenerateProfileReport(profile_output_file, profile);
  FbleFreeProfile(profile);
  return result;
}
