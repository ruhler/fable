/**
 * @file sockets.fble.c
 *  Implementation of /Network/Sockets/IO/Builtin% module.
 */

#include <assert.h>

#include <fble/fble-function.h>
#include <fble/fble-loc.h>
#include <fble/fble-module-path.h>
#include <fble/fble-name.h>
#include <fble/fble-program.h>
#include <fble/fble-string.h>
#include <fble/fble-value.h>

// -Wpedantic doesn't like our initialization of flexible array members when
// defining static FbleString values.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

static FbleString StrFile = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = __FILE__, };
static FbleString StrNetwork = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "Network", };
static FbleString StrSockets = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "Sockets", };
static FbleString StrIO = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "IO", };
static FbleString StrBuiltin = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "Builtin", };

static FbleString StrModuleBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Network/Sockets/IO/Builtin%", };

#pragma GCC diagnostic pop

#define NUM_PROFILE_BLOCKS 1

static FbleName ProfileBlocks[NUM_PROFILE_BLOCKS] = {
  { .name = &StrModuleBlock, .space = 0, .loc = { .source = &StrFile, .line = __LINE__, .col = 1 }},
};

static FbleName PathEntries[] = {
  { .name = &StrNetwork, .space = 0, .loc = { .source = &StrFile, .line = __LINE__, .col = 1 }},
  { .name = &StrSockets, .space = 0, .loc = { .source = &StrFile, .line = __LINE__, .col = 1 }},
  { .name = &StrIO, .space = 0, .loc = { .source = &StrFile, .line = __LINE__, .col = 1 }},
  { .name = &StrBuiltin, .space = 0, .loc = { .source = &StrFile, .line = __LINE__, .col = 1 }},
};

static FbleModulePath Path = {
  .refcount = 1,
  .magic = FBLE_MODULE_PATH_MAGIC,
  .loc = { .source = &StrFile, .line = __LINE__, .col = 1 },
  .path = { .size = 4, .xs = PathEntries},
};

static FbleValue* Run(FbleValueHeap* heap, FbleProfileThread* profile, FbleFunction* function, FbleValue** args)
{
  assert(false && "TODO");
  return NULL;
}

static FbleExecutable Executable = {
  .num_args = 0, 
  .num_statics = 0,
  .run = &Run,
};

FblePreloadedModule _Fble_2f_Network_2f_Sockets_2f_IO_2f_Builtin_25_ = {
  .path = &Path,
  .deps = { .size = 0, .xs = NULL },
  .executable = &Executable,
  .profile_blocks = { .size = NUM_PROFILE_BLOCKS, .xs = ProfileBlocks },
};
