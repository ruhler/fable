// md5.fble.c
//   Implementation of FbleMd5Main function.

#include <assert.h>     // for assert
#include <stdio.h>      // for FILE, printf, fprintf
#include <stdint.h>     // for uint64_t

#include <fble/fble-alloc.h>       // for FbleFree
#include <fble/fble-arg-parse.h>   // for FbleParseBoolArg, etc.
#include <fble/fble-link.h>        // for FbleCompiledModuleFunction.
#include <fble/fble-value.h>       // for FbleValue, etc.
#include <fble/fble-vector.h>      // for FbleVectorInit.
#include <fble/fble-version.h>     // for FBLE_VERSION

#include "char.fble.h"    // for FbleCharValueAccess
#include "int.fble.h"     // for FbleIntValueAccess
#include "string.fble.h"  // for FbleStringValueAccess

#define EX_SUCCESS 0
#define EX_FAILURE 1
#define EX_USAGE 2

typedef struct {
  FbleExecutable _base;
  FILE* fin;
} Executable;

static FbleValue* MkBitN(FbleValueHeap* heap, size_t n, uint64_t data);
static FbleValue* GetByte(FbleValueHeap*, FILE* fin);

static void OnFree(FbleExecutable* this);
static FbleExecStatus GetImpl(
    FbleValueHeap* heap, FbleThread* thread,
    FbleExecutable* executable,
    FbleValue** locals, FbleValue** statics,
    FbleBlockId profile_block_offset);

static void PrintUsage(FILE* stream, FbleCompiledModuleFunction* module);
int main(int argc, char* argv[]);

// MkBitN --
//  Construct a BitN fble value.
//
// Inputs:
//   heap - the heap to use for allocating the value.
//   n - the number of bits value to create. Must be a power of 2.
//   data - the raw data to use for the bits.
//
// Results:
//   A new Bit<n> value constructed from the least significant n bits of data.
//
// Side effects:
//   Allocates fblc values.
static FbleValue* MkBitN(FbleValueHeap* heap, size_t n, uint64_t data)
{
  if (n == 1) {
    return FbleNewEnumValue(heap, data & 0x1);
  }

  assert(n % 2 == 0 && "Invalid n supplied");
  int halfn = n / 2;
  FbleValue* hi = MkBitN(heap, halfn, (data >> halfn));
  FbleValue* lo = MkBitN(heap, halfn, data);
  FbleValue* result = FbleNewStructValue_(heap, 2, hi, lo);
  FbleReleaseValue(heap, hi);
  FbleReleaseValue(heap, lo);
  return result;
}

// GetByte --
//   Get the next byte from the given input stream.
static FbleValue* GetByte(FbleValueHeap* heap, FILE* fin)
{
  int c = fgetc(fin);
  if (c == EOF) {
    // Maybe<Bit8>(nothing: Unit)
    return FbleNewEnumValue(heap, 1);
  }

  // Maybe<Bit8>(just: c)
  FbleValue* byte = MkBitN(heap, 8, c);
  FbleValue* result = FbleNewUnionValue(heap, 0, byte);
  FbleReleaseValue(heap, byte);
  return result;
}

/**
 * on_free callback for GetFunctionExecutable.
 *
 * @param this  The executable to clean up.
 * @sideeffects  Frees resources associated with the Executable.
 */
static void OnFree(FbleExecutable* this)
{
  Executable* exe = (Executable*)this;
  fclose(exe->fin);
}

/**
 * Implements Md5 'get' function.
 *
 * See FbleRunFunction documentation for more info.
 */
static FbleExecStatus GetImpl(
    FbleValueHeap* heap, FbleThread* thread,
    FbleExecutable* executable,
    FbleValue** locals, FbleValue** statics,
    FbleBlockId profile_block_offset)
{
  (void)statics;
  (void)profile_block_offset;

  Executable* exe = (Executable*)executable;
  FbleValue* world = locals[0];
  FbleValue* byte = GetByte(heap, exe->fin);
  FbleValue* result = FbleNewStructValue_(heap, 2, world, byte);
  FbleReleaseValue(heap, byte);

  FbleReleaseValue(heap, locals[0]);
  return FbleThreadReturn(heap, thread, result);
}

// PrintUsage --
//   Prints help info to the given output stream.
//
// Inputs:
//   stream - The output stream to write the usage information to.
//   module - Non-NULL if a compiled module is provided, NULL otherwise.
//
// Side effects:
//   Outputs usage information to the given stream.
static void PrintUsage(FILE* stream, FbleCompiledModuleFunction* module)
{
  fprintf(stream, "Usage: fble-md5 [OPTION...]%s FILE\n",
    module == NULL ? " -m MODULE_PATH" : "");
  fprintf(stream, "%s",
      "\n"
      "Description:\n"
      "  Computes md5 hash of FILE.\n"
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
      "\n"
      "Exit Status:\n"
      "  0 on success.\n"
      "  1 on error.\n"
      "  2 on usage error.\n"
      "\n"
      "Example:\n");
  fprintf(stream, "%s%s%s",
      "  fble-md5 --profile foo.prof ",
      module == NULL ? "-I md5 -m /Md5/Main% " : "",
      "foo.txt\n");
}

// main --
//   The main entry point for fble-md5.
//
// Inputs:
//   argc - The number of command line arguments.
//   argv - The command line arguments.
//
// Results:
//   0 on success, non-zero on error.
//
// Side effects:
//   Performs IO as described in PrintUsage.
int FbleMd5Main(int argc, const char** argv, FbleCompiledModuleFunction* module)
{
  // To ease debugging of FbleStdioMain programs, cause the following useful
  // functions to be linked in:
  (void)(FbleCharValueAccess);
  (void)(FbleIntValueAccess);
  (void)(FbleStringValueAccess);

  FbleSearchPath search_path;
  FbleVectorInit(search_path);
  const char* module_path = NULL;
  const char* profile_file = NULL;
  const char* file = NULL;
  bool help = false;
  bool error = false;
  bool version = false;

  argc--;
  argv++;
  while (!(help || error || version) && argc > 0) {
    if (FbleParseBoolArg("-h", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--help", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("-v", &version, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--version", &version, &argc, &argv, &error)) continue;
    if (!module && FbleParseSearchPathArg(&search_path, &argc, &argv, &error)) continue;
    if (!module && FbleParseStringArg("-m", &module_path, &argc, &argv, &error)) continue;
    if (!module && FbleParseStringArg("--module", &module_path, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--profile", &profile_file, &argc, &argv, &error)) continue;

    // TODO: Allow users to specify multiple files?
    if (argc > 1) {
      fprintf(stderr, "Too many arguments.\n");
      error = true;
      continue;
    }

    file = argv[0];
    break;
  }

  if (version) {
    printf("fble-md5 %s\n", FBLE_VERSION);
    FbleVectorFree(search_path);
    return EX_SUCCESS;
  }

  if (help) {
    PrintUsage(stdout, module);
    FbleVectorFree(search_path);
    return EX_SUCCESS;
  }

  if (error) {
    PrintUsage(stderr, module);
    FbleVectorFree(search_path);
    return EX_USAGE;
  }

  if (!module && module_path == NULL) {
    fprintf(stderr, "missing required --module option.\n");
    PrintUsage(stderr, module);
    FbleVectorFree(search_path);
    return EX_USAGE;
  }

  if (file == NULL) {
    fprintf(stderr, "no input provided.\n");
    PrintUsage(stderr, module);
    FbleVectorFree(search_path);
    return EX_USAGE;
  }

  FILE* fprofile = NULL;
  if (profile_file != NULL) {
    fprofile = fopen(profile_file, "w");
    if (fprofile == NULL) {
      fprintf(stderr, "unable to open %s for writing.\n", profile_file);
      FbleVectorFree(search_path);
      return EX_FAILURE;
    }
  }

  FILE* fin = fopen(file, "rb");
  if (fin == NULL) {
    fprintf(stderr, "unable to open %s\n", file);
    FbleVectorFree(search_path);
    return EX_FAILURE;
  }

  FbleProfile* profile = fprofile == NULL ? NULL : FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();
  FbleValue* linked = FbleLinkFromCompiledOrSource(heap, profile, module, search_path, module_path);
  FbleVectorFree(search_path);
  if (linked == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAILURE;
  }

  // md5 has type (IO@<Maybe@<Bit8@>>) { IO@<Hash@>; }
  FbleValue* md5 = FbleEval(heap, linked, profile);
  FbleReleaseValue(heap, linked);

  FbleName block_name;
  block_name.name = FbleNewString("get!");
  block_name.loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  FbleBlockId block_id = 0;
  if (profile != NULL) {
    FbleNameV names = { .size = 1, .xs = &block_name };
    block_id = FbleProfileAddBlocks(profile, names);
  }
  FbleFreeName(block_name);

  // get_func has type IO@<Maybe@<Bit8@>>
  Executable* exec = FbleAlloc(Executable);
  exec->_base.refcount = 0;
  exec->_base.magic = FBLE_EXECUTABLE_MAGIC;
  exec->_base.num_args = 1;
  exec->_base.num_statics = 0;
  exec->_base.num_locals = 1;
  exec->_base.profile_block_id = block_id;
  exec->_base.run = &GetImpl;
  exec->_base.on_free = &OnFree;
  exec->fin = fin;
  FbleValue* get_func = FbleNewFuncValue(heap, &exec->_base, 0, NULL);

  FbleValue* computation = FbleApply(heap, md5, &get_func, profile);
  FbleReleaseValue(heap, md5);
  FbleReleaseValue(heap, get_func);

  if (computation == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAILURE;
  }

  // computation has type IO@<Hash@>, which is (World@) { R@<Hash@>; }
  FbleValue* world = FbleNewStructValue_(heap, 0);
  FbleValue* result = FbleApply(heap, computation, &world, profile);
  FbleReleaseValue(heap, computation);
  FbleReleaseValue(heap, world);

  if (result == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAILURE;
  }

  // result has type R@<Hash@>, which is *(s, x)
  FbleValue* value = FbleStructValueAccess(result, 1);

  // Print the md5 hash
  char* hex = "0123456789abcdef";
  for (size_t i = 0; i < 32; ++i) {
    FbleValue* h = FbleStructValueAccess(value, i);
    size_t tag = FbleUnionValueTag(h);
    assert(tag < 16);
    printf("%c", hex[tag]);
  }
  printf("\n");

  FbleReleaseValue(heap, result);
  FbleFreeValueHeap(heap);
  FbleFreeProfile(profile);
  return EX_SUCCESS;
}
