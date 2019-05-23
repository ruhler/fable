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
} Md5IO;

static void PrintUsage(FILE* stream);
static FbleValue* MkBitN(FbleValueArena* arena, size_t n, uint64_t data);
static bool IO(FbleIO* io, FbleValueArena* arena, bool block);
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
      "Example: fbld-md5 prgms/fble-md5.fble prgms foo.txt\n"
  );
}

// MkBitN --
//  Construct a BitN fble value.
//
// Inputs:
//   arena - the arena to use for allocating the value.
//   n - the number of bits value to create. Must be a power of 2.
//   data - the raw data to use for the bits.
//
// Results:
//   A new Bit<n> value constructed from the least significant n bits of data.
//
// Side effects:
//   Allocates fblc values.
static FbleValue* MkBitN(FbleValueArena* arena, size_t n, uint64_t data)
{
  if (n == 1) {
    FbleValueV args = { .size = 0, .xs = NULL };
    return FbleNewUnionValue(arena, data & 0x1, FbleNewStructValue(arena, args));
  }

  assert(n % 2 == 0 && "Invalid n supplied");
  int halfn = n / 2;
  FbleValue* xs[2];
  xs[1] = MkBitN(arena, halfn, data);
  xs[0] = MkBitN(arena, halfn, (data >> halfn));
  FbleValueV args = { .size = 2, .xs = xs };
  return FbleNewStructValue(arena, args);
}

// IO --
//   io function for external ports.
//   See the corresponding documentation in fble.h.
static bool IO(FbleIO* io, FbleValueArena* arena, bool block)
{
  if (block && io->ports.xs[0] == NULL) {
    // Read the next byte from the file.
    Md5IO* mio = (Md5IO*)io;
    int c = fgetc(mio->fin);
    if (c == EOF) {
      // Maybe<Bit8>:nothing(Unit())
      FbleValueV args = { .size = 0, .xs = NULL };
      FbleValue* unit = FbleNewStructValue(arena, args);
      io->ports.xs[0] = FbleNewUnionValue(arena, 1, unit);
    } else {
      // Maybe<Bit8>:just(c)
      FbleValue* byte = MkBitN(arena, 8, c);
      io->ports.xs[0] = FbleNewUnionValue(arena, 0, byte);
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
  FbleExpr* prgm = FbleParse(prgm_arena, path, include_path);
  if (prgm == NULL) {
    FbleDeleteArena(prgm_arena);
    return 1;
  }

  FbleArena* eval_arena = FbleNewArena();
  FbleValueArena* value_arena = FbleNewValueArena(eval_arena);

  FbleValue* func = FbleEval(value_arena, prgm);
  if (func == NULL) {
    FbleDeleteValueArena(value_arena);
    FbleDeleteArena(eval_arena);
    FbleDeleteArena(prgm_arena);
    return 1;
  }

  FbleValue* input = FbleNewPortValue(value_arena, 0);
  FbleValue* proc = FbleApply(value_arena, func, input);
  FbleValueRelease(value_arena, func);
  FbleValueRelease(value_arena, input);

  if (proc == NULL) {
    FbleDeleteValueArena(value_arena);
    FbleDeleteArena(eval_arena);
    FbleDeleteArena(prgm_arena);
    return 1;
  }
  assert(proc->tag == FBLE_PROC_VALUE);


  FILE* fin = fopen(file, "rb");
  if (fin == NULL) {
    fprintf(stderr, "unable to open %s\n", file);
    return 1;
  }

  FbleValue* ports[1] = {NULL};
  Md5IO mio = {
    .io = { .io = &IO, .ports = { .size = 1, .xs = ports} },
    .fin = fin
  };

  FbleValue* value = FbleExec(value_arena, &mio.io, (FbleProcValue*)proc);

  FbleValueRelease(value_arena, proc);
  assert(ports[0] == NULL);

  // Print the md5 hash
  char* hex = "0123456789abcdef";
  for (size_t i = 0; i < 32; ++i) {
    FbleValue* h = FbleStructValueAccess(value, i);
    size_t tag = FbleUnionValueTag(h);
    assert(tag < 16);
    printf("%c", hex[tag]);
  }
  printf("\n");

  FbleValueRelease(value_arena, value);
  FbleDeleteValueArena(value_arena);
  FbleAssertEmptyArena(eval_arena);
  FbleDeleteArena(eval_arena);
  FbleDeleteArena(prgm_arena);
  return 0;
}
