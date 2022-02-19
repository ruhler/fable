// md5.fble.c
//   Implementation of FbleMd5Main function.

#include <assert.h>     // for assert
#include <stdio.h>      // for FILE, printf, fprintf
#include <stdint.h>     // for uint64_t

#include "fble-alloc.h"       // for FbleFree
#include "fble-arg-parse.h"   // for FbleParseBoolArg, etc.
#include "fble-link.h"        // for FbleCompiledModuleFunction.
#include "fble-value.h"       // for FbleValue, etc.
#include "fble-vector.h"      // for FbleVectorInit.

#include "Core/char.fble.h"    // for FbleCharValueAccess
#include "Core/int.fble.h"     // for FbleIntValueAccess
#include "Core/string.fble.h"  // for FbleStringValueAccess

#define EX_SUCCESS 0
#define EX_FAILURE 1
#define EX_USAGE 2

// Md5IO --
//   The FbleIO for Md5.
typedef struct {
  FbleIO io;
  FILE* fin;

  FbleValue* input;
} Md5IO;

static FbleValue* MkBitN(FbleValueHeap* heap, size_t n, uint64_t data);
static bool IO(FbleIO* io, FbleValueHeap* heap, bool block);
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
  FbleValue* result = FbleNewStructValue(heap, 2, hi, lo);
  FbleReleaseValue(heap, hi);
  FbleReleaseValue(heap, lo);
  return result;
}

// IO --
//   FbleIo.io function for external ports.
//   See the corresponding documentation in fble-value.h.
static bool IO(FbleIO* io, FbleValueHeap* heap, bool block)
{
  Md5IO* mio = (Md5IO*)io;
  if (block && mio->input == NULL) {
    // Read the next byte from the file.
    int c = fgetc(mio->fin);
    if (c == EOF) {
      // Maybe<Bit8>:nothing(Unit())
      mio->input = FbleNewEnumValue(heap, 1);
    } else {
      // Maybe<Bit8>:just(c)
      FbleValue* byte = MkBitN(heap, 8, c);
      mio->input = FbleNewUnionValue(heap, 0, byte);
      FbleReleaseValue(heap, byte);
    }
    return true;
  }
  return false;
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
      "     Print this help message and exit.\n");
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
      module == NULL ? "-I md5 -m /Md5/Md5% " : "",
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

  argc--;
  argv++;
  while (!error && argc > 0) {
    if (FbleParseBoolArg("-h", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--help", &help, &argc, &argv, &error)) continue;
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

  if (help) {
    PrintUsage(stdout, module);
    FbleFree(search_path.xs);
    return EX_SUCCESS;
  }

  if (error) {
    PrintUsage(stderr, module);
    FbleFree(search_path.xs);
    return EX_USAGE;
  }

  if (!module && module_path == NULL) {
    fprintf(stderr, "missing required --module option.\n");
    PrintUsage(stderr, module);
    FbleFree(search_path.xs);
    return EX_USAGE;
  }

  if (file == NULL) {
    fprintf(stderr, "no input provided.\n");
    PrintUsage(stderr, module);
    FbleFree(search_path.xs);
    return EX_USAGE;
  }

  FILE* fprofile = NULL;
  if (profile_file != NULL) {
    fprofile = fopen(profile_file, "w");
    if (fprofile == NULL) {
      fprintf(stderr, "unable to open %s for writing.\n", profile_file);
      FbleFree(search_path.xs);
      return EX_FAILURE;
    }
  }

  FbleProfile* profile = fprofile == NULL ? NULL : FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();
  FbleValue* md5 = FbleLinkFromCompiledOrSource(heap, profile, module, search_path, module_path);
  FbleFree(search_path.xs);
  if (md5 == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAILURE;
  }

  FbleValue* func = FbleEval(heap, md5, NULL);
  FbleReleaseValue(heap, md5);

  if (func == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAILURE;
  }

  FILE* fin = fopen(file, "rb");
  if (fin == NULL) {
    fprintf(stderr, "unable to open %s\n", file);
    FbleReleaseValue(heap, func);
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAILURE;
  }

  Md5IO mio = {
    .io = { .io = &IO, },
    .fin = fin,
    .input = NULL,
  };

  FbleValue* input = FbleNewInputPortValue(heap, &mio.input, 0);
  FbleValue* proc = FbleApply(heap, func, &input, NULL);
  FbleReleaseValue(heap, func);
  FbleReleaseValue(heap, input);

  if (proc == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAILURE;
  }

  FbleValue* value = FbleExec(heap, &mio.io, proc, NULL);

  FbleReleaseValue(heap, proc);
  assert(mio.input == NULL);

  // Print the md5 hash
  char* hex = "0123456789abcdef";
  for (size_t i = 0; i < 32; ++i) {
    FbleValue* h = FbleStructValueAccess(value, i);
    size_t tag = FbleUnionValueTag(h);
    assert(tag < 16);
    printf("%c", hex[tag]);
  }
  printf("\n");

  FbleReleaseValue(heap, value);
  FbleFreeValueHeap(heap);
  FbleFreeProfile(profile);
  return EX_SUCCESS;
}
