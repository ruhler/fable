/**
 * @file debug.fble.c
 *  Implementation of /Core/Debug/Native% module.
 */

#include <fble/fble-function.h>
#include <fble/fble-module-path.h>   // for FbleModulePath, etc.
#include <fble/fble-program.h>
#include <fble/fble-value.h>         // for FbleValue, etc.

#include "string.fble.h"             // For FbleDebugTrace

// -Wpedantic doesn't like our initialization of flexible array members when
// defining static FbleString values.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

static FbleString Filename = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = __FILE__, };
static FbleString StrCore = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "Core", };
static FbleString StrDebug = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "Debug", };
static FbleString StrNative = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "Native", };
static FbleString StrModuleBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Debug/Core/Native%" };
static FbleString StrTraceBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Debug/Core/Native%.Trace" };

#pragma GCC diagnostic pop

static FbleName PathEntries[] = {
  { .name = &StrCore, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrDebug, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrNative, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
};

static FbleModulePath Path = {
  .refcount = 1,
  .magic = FBLE_MODULE_PATH_MAGIC,
  .loc = { .source = &Filename, .line = __LINE__, .col = 1 },
  .path = { .size = 3, .xs = PathEntries},
};

static FbleName ProfileBlocks[] = {
  { .name = &StrModuleBlock, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrTraceBlock, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
};

static FbleValue* TraceImpl(FbleValueHeap* heap, FbleProfileThread* profile, FbleFunction* function, FbleValue** args)
{
  FbleDebugTrace(args[0]);
  return FbleNewStructValue_(heap, 0);
}

static FbleValue* Run(FbleValueHeap* heap, FbleProfileThread* profile, FbleFunction* function, FbleValue** args)
{
  FblePushFrame(heap);
  FbleExecutable trace_exe = {
    .num_args = 1,
    .num_statics = 0,
    .run = &TraceImpl,
  };

  FbleValue* trace = FbleNewFuncValue(heap, &trace_exe, 1, NULL);
  FbleValue* native = FbleNewStructValue_(heap, 1, trace);
  return FblePopFrame(heap, native);
}

static FbleExecutable Executable = {
  .num_args = 0, 
  .num_statics = 0,
  .run = &Run,
};

FblePreloadedModule _Fble_2f_Core_2f_Debug_2f_Native_25_ = {
  .path = &Path,
  .deps = { .size = 0, .xs = NULL},
  .executable = &Executable,
  .profile_blocks = { .size = 2, .xs = ProfileBlocks },
};

