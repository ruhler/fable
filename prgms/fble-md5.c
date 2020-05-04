// fble-md5
//   A program to compute the md5 sum of a file using an fble implementation.

#include <assert.h>     // for assert
#include <stdio.h>      // for FILE, printf, fprintf
#include <stdlib.h>     // for malloc, free
#include <stdint.h>     // for uint64_t
#include <string.h>     // for strcmp

#include "fble.h"

// Md5IO --
//   The FbleIO for Md5.
typedef struct {
  FbleIO io;
  FILE* fin;

  FbleValue* input;
} Md5IO;

static void PrintUsage(FILE* stream);
static FbleValue* MkBitN(FbleValueHeap* heap, size_t n, uint64_t data);
static bool IO(FbleIO* io, FbleValueHeap* heap, bool block);
int main(int argc, char* argv[]);

// PrintUsage --
//   Prints help info to the given output stream.
//
// Inputs:
//   stream - The output stream to write the usage information to.
//
// Result:
//   None.
//
// Side effects:
//   Outputs usage information to the given stream.
static void PrintUsage(FILE* stream)
{
  fprintf(stream,
      "Usage: fbld-md5 PRGM PATH FILE \n"
      "Execute the md5 process described by the fble program PRGM.\n"
      "Using search path PATH, and computing the md5 of FILE.\n"
      "Example: fbld-md5 prgms/Md5/Stdio.fble prgms foo.txt\n"
  );
}

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
    FbleValueV args = { .size = 0, .xs = NULL };
    return FbleNewUnionValue(heap, data & 0x1, FbleNewStructValue(heap, args));
  }

  assert(n % 2 == 0 && "Invalid n supplied");
  int halfn = n / 2;
  FbleValue* xs[2];
  xs[1] = MkBitN(heap, halfn, data);
  xs[0] = MkBitN(heap, halfn, (data >> halfn));
  FbleValueV args = { .size = 2, .xs = xs };
  return FbleNewStructValue(heap, args);
}

// IO --
//   io function for external ports.
//   See the corresponding documentation in fble.h.
static bool IO(FbleIO* io, FbleValueHeap* heap, bool block)
{
  Md5IO* mio = (Md5IO*)io;
  if (block && mio->input == NULL) {
    // Read the next byte from the file.
    int c = fgetc(mio->fin);
    if (c == EOF) {
      // Maybe<Bit8>:nothing(Unit())
      FbleValueV args = { .size = 0, .xs = NULL };
      FbleValue* unit = FbleNewStructValue(heap, args);
      mio->input = FbleNewUnionValue(heap, 1, unit);
    } else {
      // Maybe<Bit8>:just(c)
      FbleValue* byte = MkBitN(heap, 8, c);
      mio->input = FbleNewUnionValue(heap, 0, byte);
    }
    return true;
  }
  return false;
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
int main(int argc, char* argv[])
{
  if (argc > 1 && strcmp("--help", argv[1]) == 0) {
    PrintUsage(stdout);
    return 0;
  }

  if (argc <= 1) {
    fprintf(stderr, "no input program.\n");
    PrintUsage(stderr);
    return 1;
  }

  if (argc <= 2) {
    fprintf(stderr, "no include path provided.\n");
    PrintUsage(stderr);
    return 1;
  }

  if (argc <= 3) {
    fprintf(stderr, "no input file.\n");
    PrintUsage(stderr);
    return 1;
  }

  const char* path = argv[1];
  const char* include_path = argv[2];
  const char* file = argv[3];

  FbleArena* prgm_arena = FbleNewArena();
  FbleProgram* prgm = FbleLoad(prgm_arena, path, include_path);
  if (prgm == NULL) {
    FbleFreeArena(prgm_arena);
    return 1;
  }

  FbleArena* eval_arena = FbleNewArena();
  FbleValueHeap* heap = FbleNewValueHeap(eval_arena);
  FbleProfile* profile = FbleNewProfile(eval_arena);

  FbleValue* func = FbleEval(heap, prgm, profile);
  if (func == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(eval_arena, profile);
    FbleFreeArena(eval_arena);
    FbleFreeArena(prgm_arena);
    return 1;
  }

  FILE* fin = fopen(file, "rb");
  if (fin == NULL) {
    fprintf(stderr, "unable to open %s\n", file);
    return 1;
  }

  Md5IO mio = {
    .io = { .io = &IO, },
    .fin = fin,
    .input = NULL,
  };

  FbleValue* input = FbleNewInputPortValue(heap, &mio.input);
  FbleValue* proc = FbleApply(heap, func, &input, profile);
  FbleValueRelease(heap, func);
  FbleValueRelease(heap, input);

  if (proc == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(eval_arena, profile);
    FbleFreeArena(eval_arena);
    FbleFreeArena(prgm_arena);
    return 1;
  }

  FbleValue* value = FbleExec(heap, &mio.io, proc, profile);

  FbleValueRelease(heap, proc);
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

  FbleValueRelease(heap, value);
  FbleFreeValueHeap(heap);
  FbleFreeProfile(eval_arena, profile);
  FbleAssertEmptyArena(eval_arena);
  FbleFreeArena(eval_arena);
  FbleFreeArena(prgm_arena);
  return 0;
}
