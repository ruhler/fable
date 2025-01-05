/**
 * @file builtin.c
 *  Implementation of /SpecTests/Builtin% module.
 */

#include <fble/fble-function.h>
#include <fble/fble-module-path.h>   // for FbleModulePath, etc.
#include <fble/fble-program.h>
#include <fble/fble-value.h>         // for FbleValue, etc.

// -Wpedantic doesn't like our initialization of flexible array members when
// defining static FbleString values.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

static FbleString Filename = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = __FILE__, };
static FbleString StrSpecTests = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "SpecTests", };
static FbleString StrBuiltin = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "Builtin", };
static FbleString StrModuleBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/SpecTests/Builtin%" };
static FbleString StrIdBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/SpecTests/Builtin%.Id" };

#pragma GCC diagnostic pop

static FbleName PathEntries[] = {
  { .name = &StrSpecTests, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrBuiltin, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
};

static FbleModulePath Path = {
  .refcount = 1,
  .magic = FBLE_MODULE_PATH_MAGIC,
  .loc = { .source = &Filename, .line = __LINE__, .col = 1 },
  .path = { .size = 2, .xs = PathEntries},
};

static FbleName ProfileBlocks[] = {
  { .name = &StrModuleBlock, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrIdBlock, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
};

static FbleValue* IdImpl(FbleValueHeap* heap, FbleProfileThread* profile, FbleFunction* function, FbleValue** args)
{
  return args[0];
}

static FbleValue* Run(FbleValueHeap* heap, FbleProfileThread* profile, FbleFunction* function, FbleValue** args)
{
  FblePushFrame(heap);
  FbleExecutable id_exe = {
    .num_args = 1,
    .num_statics = 0,
    .run = &IdImpl,
  };

  FbleValue* id = FbleNewFuncValue(heap, &id_exe, 1, NULL);
  FbleValue* builtin = FbleNewStructValue_(heap, 2, id, id);
  return FblePopFrame(heap, builtin);
}

static FbleExecutable Executable = {
  .num_args = 0, 
  .num_statics = 0,
  .run = &Run,
};

FblePreloadedModule _Fble_2f_SpecTests_2f_Builtin_25_ = {
  .path = &Path,
  .deps = { .size = 0, .xs = NULL },
  .executable = &Executable,
  .profile_blocks = { .size = 2, .xs = ProfileBlocks },
};
