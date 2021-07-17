// fble-md5
//   A program to compute the md5 sum of a file using an fble implementation.

#include <assert.h>     // for assert
#include <stdio.h>      // for FILE, printf, fprintf
#include <stdlib.h>     // for malloc, free
#include <stdint.h>     // for uint64_t
#include <string.h>     // for strcmp

#include "fble-main.h"    // for FbleMain
#include "fble-value.h"   // for FbleValue, etc.

// Md5IO --
//   The FbleIO for Md5.
typedef struct {
  FbleIO io;
  FILE* fin;

  FbleValue input;
} Md5IO;

static void PrintUsage(FILE* stream);
static FbleValue MkBitN(FbleValueHeap* heap, size_t n, uint64_t data);
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
  fprintf(stream, "%s",
      "Usage: fble-md5 FILE " FBLE_MAIN_USAGE_SUMMARY "\n"
      "Compute md5 on FILE using the given fble program.\n"
      FBLE_MAIN_USAGE_DETAIL
      "Example: fble-md5 foo.txt " FBLE_MAIN_USAGE_EXAMPLE "\n"
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
static FbleValue MkBitN(FbleValueHeap* heap, size_t n, uint64_t data)
{
  if (n == 1) {
    return FbleNewEnumValue(heap, data & 0x1);
  }

  assert(n % 2 == 0 && "Invalid n supplied");
  int halfn = n / 2;
  FbleValue hi = MkBitN(heap, halfn, (data >> halfn));
  FbleValue lo = MkBitN(heap, halfn, data);
  FbleValue result = FbleNewStructValue(heap, 2, hi, lo);
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
  if (block && FbleValueIsNull(mio->input)) {
    // Read the next byte from the file.
    int c = fgetc(mio->fin);
    if (c == EOF) {
      // Maybe<Bit8>:nothing(Unit())
      mio->input = FbleNewEnumValue(heap, 1);
    } else {
      // Maybe<Bit8>:just(c)
      FbleValue byte = MkBitN(heap, 8, c);
      mio->input = FbleNewUnionValue(heap, 0, byte);
      FbleReleaseValue(heap, byte);
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
  const char* file = argv[1];

  FbleValueHeap* heap = FbleNewValueHeap();
  FbleValue linked = FbleMain(heap, NULL, FbleCompiledMain, argc - 2, argv + 2);
  if (FbleValueIsNull(linked)) {
    FbleFreeValueHeap(heap);
    return 1;
  }

  FbleValue func = FbleEval(heap, linked, NULL);
  FbleReleaseValue(heap, linked);

  if (FbleValueIsNull(func)) {
    FbleFreeValueHeap(heap);
    return 1;
  }

  FILE* fin = fopen(file, "rb");
  if (fin == NULL) {
    fprintf(stderr, "unable to open %s\n", file);
    FbleReleaseValue(heap, func);
    FbleFreeValueHeap(heap);
    return 1;
  }

  Md5IO mio = {
    .io = { .io = &IO, },
    .fin = fin,
    .input = FbleNullValue,
  };

  FbleValue input = FbleNewInputPortValue(heap, &mio.input, 0);
  FbleValue proc = FbleApply(heap, func, &input, NULL);
  FbleReleaseValue(heap, func);
  FbleReleaseValue(heap, input);

  if (FbleValueIsNull(proc)) {
    FbleFreeValueHeap(heap);
    return 1;
  }

  FbleValue value = FbleExec(heap, &mio.io, proc, NULL);

  FbleReleaseValue(heap, proc);
  assert(FbleValueIsNull(mio.input));

  // Print the md5 hash
  char* hex = "0123456789abcdef";
  for (size_t i = 0; i < 32; ++i) {
    FbleValue h = FbleStructValueAccess(value, i);
    size_t tag = FbleUnionValueTag(h);
    assert(tag < 16);
    printf("%c", hex[tag]);
  }
  printf("\n");

  FbleReleaseValue(heap, value);
  FbleFreeValueHeap(heap);
  return 0;
}
