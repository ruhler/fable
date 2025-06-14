/**
 * @file stdio.fble.c
 *  Implementation of /Core/Stdio/IO/Builtin%, FbleStdio and FbleStdioMain
 *  functions.
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
#include "debug.fble.h"       // for /Core/Debug/Builtin%
#include "int.fble.h"         // for FbleNewIntValue, FbleIntValueAccess
#include "string.fble.h"      // for FbleNewStringValue, FbleStringValueAccess

#define MAYBE_TAGWIDTH 1
#define BOOL_TAGWIDTH 1
#define LIST_TAGWIDTH 1
#define RESULT_FIELDC 2

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


// -Wpedantic doesn't like our initialization of flexible array members when
// defining static FbleString values.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

static FbleString Filename = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = __FILE__, };
static FbleString StrCore = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "Core", };
static FbleString StrStdio = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "Stdio", };
static FbleString StrIO = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "IO", };
static FbleString StrBuiltin = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "Builtin", };

static FbleString StrModuleBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Core/Stdio/IO/Builtin%", };
static FbleString StrIStreamBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Core/Stdio/IO/Builtin%.IStream", };
static FbleString StrOStreamBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Core/Stdio/IO/Builtin%.OStream", };
static FbleString StrReadBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Core/Stdio/IO/Builtin%.Read", };
static FbleString StrWriteBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Core/Stdio/IO/Builtin%.Write", };
static FbleString StrGetEnvBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Core/Stdio/IO/Builtin%.GetEnv", };

#pragma GCC diagnostic pop

static FbleName ProfileBlocks[] = {
  { .name = &StrModuleBlock, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrIStreamBlock, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrOStreamBlock, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrReadBlock, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrWriteBlock, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrGetEnvBlock, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
};

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
    ms = FbleNewEnumValue(heap, MAYBE_TAGWIDTH, 1);
  } else {
    FbleValue* v = FbleNewIntValue(heap, c);
    ms = FbleNewUnionValue(heap, MAYBE_TAGWIDTH, 0, v);
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
  if (c == -1) {
    fclose(file);
  } else {
    fputc(c, file);
    fflush(file);
  }

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
    mstream = FbleNewEnumValue(heap, MAYBE_TAGWIDTH, 1); // Nothing
  } else {
    // The Read block id is index 3 from the main block id
    FbleBlockId module_block_id = function->profile_block_id - 3;
    FbleValue* stream = IStream(heap, fin, module_block_id);
    mstream = FbleNewUnionValue(heap, MAYBE_TAGWIDTH, 0, stream); // Just(stream)
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
    mstream = FbleNewEnumValue(heap, MAYBE_TAGWIDTH, 1); // Nothing
  } else {
    // The Write block id is index 4 from the main block id
    FbleBlockId module_block_id = function->profile_block_id - 4;
    FbleValue* stream = OStream(heap, fin, module_block_id);
    mstream = FbleNewUnionValue(heap, MAYBE_TAGWIDTH, 0, stream); // Just(stream)
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
    mstr = FbleNewEnumValue(heap, MAYBE_TAGWIDTH, 1); // Nothing
  } else {
    FbleValue* str = FbleNewStringValue(heap, value);
    mstr = FbleNewUnionValue(heap, MAYBE_TAGWIDTH, 0, str); // Just(str)
  }

  return FbleNewStructValue_(heap, 2, world, mstr);
}

/**
 * @func[IStream] Allocates an @l{IStream@} for a file.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FILE*][file] The FILE to allocate the @l{IStream@} for.
 *  @arg[FbleBlockId][module_block_id]
 *   The block_id of the /Core/Stdio/IO/Builtin% block.
 *
 *  @returns[FbleValue*] An fble @l{IStream@} function value.
 *
 *  @sideeffects
 *   Allocates a value on the heap.
 */
static FbleValue* IStream(FbleValueHeap* heap, FILE* file, FbleBlockId module_block_id)
{
  FbleValue* native = FbleNewNativeValue(heap, file, NULL);

  FbleExecutable exe = {
    .num_args = 1,
    .num_statics = 1,
    .max_call_args = 0,
    .run = &IStreamImpl,
  };

  return FbleNewFuncValue(heap, &exe, module_block_id + 1, &native);
}

/**
 * @func[OStream] Allocates an @l{OStream@} for a file.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FILE*][file] The FILE to allocate the @l{OStream@} for.
 *  @arg[FbleBlockId][module_block_id]
 *   The block_id of the /Core/Stdio/IO/Builtin% block.
 *
 *  @returns[FbleValue*] An fble @l{OStream@} function value.
 *
 *  @sideeffects
 *   Allocates a value on the heap.
 */
static FbleValue* OStream(FbleValueHeap* heap, FILE* file, FbleBlockId module_block_id)
{
  FbleValue* native = FbleNewNativeValue(heap, file, NULL);

  FbleExecutable exe = {
    .num_args = 2,
    .num_statics = 1,
    .max_call_args = 0,
    .run = &OStreamImpl,
  };

  return FbleNewFuncValue(heap, &exe, module_block_id + 2, &native);
}

/**
 * @func[Read] Allocates an fble read function.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleBlockId][module_block_id]
 *   The block_id of the /Core/Stdio/IO/Builtin% block.
 *  @returns[FbleValue*] The allocated function.
 *  @sideeffects Allocates an fble value.
 */
static FbleValue* Read(FbleValueHeap* heap, FbleBlockId module_block_id)
{
  FbleExecutable exe = {
    .num_args = 2,
    .num_statics = 0,
    .max_call_args = 0,
    .run = &ReadImpl,
  };
  return FbleNewFuncValue(heap, &exe, module_block_id + 3, NULL);
}

/**
 * @func[Write] Allocates an fble write function.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleBlockId][module_block_id]
 *   The block_id of the /Core/Stdio/IO/Builtin% block.
 *  @returns[FbleValue*] The allocated function.
 *  @sideeffects Allocates an fble value.
 */
static FbleValue* Write(FbleValueHeap* heap, FbleBlockId module_block_id)
{
  FbleExecutable exe = {
    .num_args = 2,
    .num_statics = 0,
    .max_call_args = 0,
    .run = &WriteImpl,
  };
  return FbleNewFuncValue(heap, &exe, module_block_id + 4, NULL);
}

/**
 * @func[GetEnv] Allocates an fble getenv function.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleBlockId][module_block_id]
 *   The block_id of the /Core/Stdio/IO/Builtin% block.
 *  @returns[FbleValue*] The allocated function.
 *  @sideeffects Allocates an fble value.
 */
static FbleValue* GetEnv(FbleValueHeap* heap, FbleBlockId module_block_id)
{
  FbleExecutable exe = {
    .num_args = 2,
    .num_statics = 0,
    .max_call_args = 0,
    .run = &GetEnvImpl,
  };
  return FbleNewFuncValue(heap, &exe, module_block_id + 5, NULL);
}


static FbleName Core_Stdio_IO_Builtin_PathEntries[] = {
  { .name = &StrCore, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrStdio, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrIO, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrBuiltin, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
};

static FbleModulePath Core_Stdio_IO_Builtin_Path = {
  .refcount = 1,
  .magic = FBLE_MODULE_PATH_MAGIC,
  .loc = { .source = &Filename, .line = __LINE__, .col = 1 },
  .path = { .size = 4, .xs = Core_Stdio_IO_Builtin_PathEntries},
};

static FbleValue* Core_Stdio_IO_Builtin_Run(FbleValueHeap* heap, FbleProfileThread* profile, FbleFunction* function, FbleValue** args)
{
  FbleBlockId block_id = function->profile_block_id;

  FblePushFrame(heap);
  FbleValue* fble_stdin = IStream(heap, stdin, block_id);
  FbleValue* fble_stdout = OStream(heap, stdout, block_id);
  FbleValue* fble_stderr = OStream(heap, stderr, block_id);
  FbleValue* fble_read = Read(heap, block_id);
  FbleValue* fble_write = Write(heap, block_id);
  FbleValue* fble_getenv = GetEnv(heap, block_id);
  FbleValue* fble_stdio = FbleNewStructValue_(heap, 6,
      fble_stdin, fble_stdout, fble_stderr,
      fble_read, fble_write, fble_getenv);
  return FblePopFrame(heap, fble_stdio);
}

static FbleExecutable Core_Stdio_IO_Builtin_Executable = {
  .num_args = 0, 
  .num_statics = 0,
  .max_call_args = 0,
  .run = &Core_Stdio_IO_Builtin_Run,
};

FblePreloadedModule _Fble_2f_Core_2f_Stdio_2f_IO_2f_Builtin_25_ = {
  .path = &Core_Stdio_IO_Builtin_Path,
  .deps = { .size = 0, .xs = NULL },
  .executable = &Core_Stdio_IO_Builtin_Executable,
  .profile_blocks = { .size = 6, .xs = ProfileBlocks },
};

// FbleStdio -- see documentation in stdio.fble.h
FbleValue* FbleStdio(FbleValueHeap* heap, FbleProfile* profile, FbleValue* stdio, size_t argc, FbleValue** argv)
{
  FbleValue* func = FbleEval(heap, stdio, profile);
  if (func == NULL) {
    return NULL;
  }

  FblePushFrame(heap);
  FbleValue* argS = FbleNewEnumValue(heap, LIST_TAGWIDTH, 1);
  for (size_t i = 0; i < argc; ++i) {
    FbleValue* argP = FbleNewStructValue_(heap, 2, argv[argc - i -1], argS);
    argS = FbleNewUnionValue(heap, LIST_TAGWIDTH, 0, argP);
  }

  FbleValue* args[1] = { argS };
  FbleValue* computation = FbleApply(heap, func, 1, args, profile);

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
  FbleValue* value = FbleStructValueField(result, RESULT_FIELDC, 1);
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

  FbleProfile* profile = FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();
  FILE* profile_output_file = NULL;
  FbleValue* stdio = NULL;

  FblePreloadedModuleV builtins;
  FbleInitVector(builtins);
  FbleAppendToVector(builtins, &_Fble_2f_Core_2f_Debug_2f_Builtin_25_);
  FbleAppendToVector(builtins, &_Fble_2f_Core_2f_Stdio_2f_IO_2f_Builtin_25_);

  FbleMainStatus status = FbleMain(NULL, NULL, "fble-stdio", fbldUsageHelpText,
      &argc, &argv, preloaded, builtins, heap, profile, &profile_output_file, &stdio);

  FbleFreeVector(builtins);

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

  FbleMainStatus result = FBLE_MAIN_OTHER_ERROR;
  if (value != NULL) {
    result = (FbleUnionValueTag(value, BOOL_TAGWIDTH) == 0) ? FBLE_MAIN_SUCCESS : FBLE_MAIN_FAILURE;
  }

  FbleFreeValueHeap(heap);

  FbleOutputProfile(profile_output_file, profile);
  FbleFreeProfile(profile);
  return result;
}
