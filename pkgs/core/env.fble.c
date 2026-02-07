/**
 * @file env.fble.c
 *  Implementation of /Core/Env/Native% module.
 */

#include "env.fble.h"

#include <stdlib.h>									 // for getenv
#include <string.h>                  // for strchr

#include <fble/fble-alloc.h>         // for FbleFree
#include <fble/fble-function.h>
#include <fble/fble-module-path.h>   // for FbleModulePath, etc.
#include <fble/fble-program.h>
#include <fble/fble-value.h>         // for FbleValue, etc.

#include "string.fble.h"             // For FbleNewStringValue.

#define MAYBE_TAGWIDTH 1

// -Wpedantic doesn't like our initialization of flexible array members when
// defining static FbleString values.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

static FbleString Filename = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = __FILE__, };
static FbleString StrCore = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "Core", };
static FbleString StrEnv = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "Env", };
static FbleString StrNative = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "Native", };
static FbleString StrModuleBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Core/Env/Native%" };
static FbleString StrGetVarBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Core/Env/Native%.GetVar" };

#pragma GCC diagnostic pop

static FbleName PathEntries[] = {
  { .name = &StrCore, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrEnv, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrNative, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
};

static FbleModulePath Path = {
  .refcount = 1,
  .magic = FBLE_MODULE_PATH_MAGIC,
  .loc = { .source = &Filename, .line = __LINE__, .col = 1 },
  .path = { .size = 3, .xs = PathEntries},
};

#define GETVAR_BLOCK_OFFSET 1
#define NUM_PROFILE_BLOCKS 2

static FbleName ProfileBlocks[] = {
  { .name = &StrModuleBlock, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrGetVarBlock, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
};

/**
 * @func[GetVarImpl] FbleRunFunction to read environment variables.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (Native@<M@>, Monad@<M@>, String@, Unit@) { Maybe@<String@>; }
 */
static FbleValue* GetVarImpl(FbleValueHeap* heap, FbleProfileThread* profile, FbleFunction* function, FbleValue** args)
{
  char* var = FbleStringValueAccess(args[2]);
  char* value = getenv(var);
  FbleFree(var);

  if (value == NULL) {
    return FbleNewEnumValue(heap, MAYBE_TAGWIDTH, 1); // Nothing
  }

  FbleValue* str = FbleNewStringValue(heap, value);
  return FbleNewUnionValue(heap, MAYBE_TAGWIDTH, 0, str); // Just(str)
}

static FbleValue* Run(FbleValueHeap* heap, FbleProfileThread* profile, FbleFunction* function, FbleValue** args)
{
  FblePushFrame(heap);
  FbleExecutable getvar_exe = {
    .num_args = 4,
    .num_statics = 0,
    .max_call_args = 0,
    .run = &GetVarImpl,
  };

  FbleValue* getvar = FbleNewFuncValue(heap, &getvar_exe, function->profile_block_id + GETVAR_BLOCK_OFFSET, NULL);
  FbleValue* native = FbleNewStructValue_(heap, 1, getvar);
  return FblePopFrame(heap, native);
}

static FbleExecutable Executable = {
  .num_args = 0, 
  .num_statics = 0,
  .max_call_args = 0,
  .run = &Run,
};

FblePreloadedModule _Fble_2f_Core_2f_Env_2f_Native_25_ = {
  .path = &Path,
  .deps = { .size = 0, .xs = NULL},
  .executable = &Executable,
  .profile_blocks = { .size = NUM_PROFILE_BLOCKS, .xs = ProfileBlocks },
};

