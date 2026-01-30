/**
 * @file env.fble.c
 *  Implementation of /Core/Env/Native% module.
 */

#include "env.fble.h"

#include <string.h>                  // for strchr

#include <fble/fble-function.h>
#include <fble/fble-module-path.h>   // for FbleModulePath, etc.
#include <fble/fble-program.h>
#include <fble/fble-value.h>         // for FbleValue, etc.

#include "string.fble.h"             // For FbleNewStringValue.

extern char **environ;

#define LIST_TAGWIDTH 1

// -Wpedantic doesn't like our initialization of flexible array members when
// defining static FbleString values.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

static FbleString Filename = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = __FILE__, };
static FbleString StrCore = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "Core", };
static FbleString StrEnv = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "Env", };
static FbleString StrNative = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "Native", };
static FbleString StrModuleBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Debug/Env/Native%" };
static FbleString StrGetEnvBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Debug/Env/Native%.GetEnv" };

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

#define GETENV_BLOCK_OFFSET 1
#define NUM_PROFILE_BLOCKS 2

static FbleName ProfileBlocks[] = {
  { .name = &StrModuleBlock, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrGetEnvBlock, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
};

/**
 * @func[GetEnvImpl] FbleRunFunction to read environment variables.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   @ Entry@ = *(String@ name, String@ value);
 *   @ Env@ = List@<Entry@>;
 *   (Native@<M@>, Monad@<M@>, Unit@) { Env@; }
 */
static FbleValue* GetEnvImpl(FbleValueHeap* heap, FbleProfileThread* profile, FbleFunction* function, FbleValue** args)
{
  // Note: Grab the the environ pointer at the start and make sure we keep
  // using that same pointer throughout the iteration to reduce the risk of
  // error from simultaneous modification to the environment.
  FbleValue* argS = FbleNewEnumValue(heap, LIST_TAGWIDTH, 1);
  for (char** envp = environ; *envp != NULL; ++envp) {
    char* split = strchr(*envp, '=');
    if (split == NULL) {
      // Something funny going on here. Ignore this variable.
      continue;
    }

    FbleValue* key = FbleNewSubStringValue(heap, *envp, split - *envp);
    FbleValue* value = FbleNewStringValue(heap, split+1);
    FbleValue* entry = FbleNewStructValue_(heap, 2, key, value);
    FbleValue* argP = FbleNewStructValue_(heap, 2, entry, argS);
    argS = FbleNewUnionValue(heap, LIST_TAGWIDTH, 0, argP);
  }

  // Note: This returns the list in reverse order from environ. Does anyone
  // care? If so, we should reverse it somewhere.
  return argS;
}

static FbleValue* Run(FbleValueHeap* heap, FbleProfileThread* profile, FbleFunction* function, FbleValue** args)
{
  FblePushFrame(heap);
  FbleExecutable getenv_exe = {
    .num_args = 3,
    .num_statics = 0,
    .max_call_args = 0,
    .run = &GetEnvImpl,
  };

  FbleValue* getenv = FbleNewFuncValue(heap, &getenv_exe, function->profile_block_id + GETENV_BLOCK_OFFSET, NULL);
  FbleValue* native = FbleNewStructValue_(heap, 1, getenv);
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

