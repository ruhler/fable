// fbld-md5
//   A program to compute the md5 sum of a file using an fbld implementation.

#include <assert.h>     // for assert
#include <stdio.h>      // for FILE, printf, fprintf
#include <stdlib.h>     // for malloc, free
#include <string.h>     // for strcmp

#include "fblc.h"
#include "fbld.h"

// IOUser --
//   User data for FblcIO.
typedef struct {
  FILE* fin;
} IOUser;

static void PrintUsage(FILE* stream);
static void IO(void* user, FblcArena* arena, bool block, FblcValue** ports);
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
      "Usage: fbld-md5 PATH MAIN FILE \n"
      "Execute the md5 process called MAIN in the environment of the\n"
      "fbld program PATH.\n"
      "The contents of FILE are streamed to the md5 process.\n"
      "Example: fbld-md5 prgms Md5@Md5@Md5 foo.txt\n"
  );
}

// IO --
//   io function for external ports with IOUser as user data.
//   See the corresponding documentation in fblc.h.
static void IO(void* user, FblcArena* arena, bool block, FblcValue** ports)
{
  IOUser* io_user = (IOUser*)user;
  if (block && ports[0] == NULL) {
    // Read the next byte from the file.
    int c = fgetc(io_user->fin);
    if (c == EOF) {
      // Maybe<Bit8>:nothing(Unit())
      ports[0] = FblcNewUnion(arena, 2, 1, FblcNewStruct(arena, 0));
    } else {
      FblcValue* byte = FblcNewStruct(arena, 8);
      for (size_t i = 0; i < 8; ++i) {
        int bit = c & 0x01;
        byte->fields[7 - i] = FblcNewUnion(arena, 2, bit, FblcNewStruct(arena, 0));
        c >>= 1;
      }
      // Maybe<Bit8>:just(c)
      ports[0] = FblcNewUnion(arena, 2, 0, byte);
    }
  }
}

// main --
//   The main entry point for fblc-md5.
//
// Inputs:
//   argc - The number of command line arguments.
//   argv - The command line arguments.
//
// Results:
//   0 on success, non-zero on error.
//
// Side effects:
//   Performs IO based on the execution of the MAIN process on the
//   given arguments, or prints an error message to standard error if an error
//   is encountered.
//
//   This function leaks memory under the assumption it is called as the main
//   entry point of the program and the OS will free the memory resources used
//   immediately after the function returns.
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
    fprintf(stderr, "no main entry point provided.\n");
    PrintUsage(stderr);
    return 1;
  }

  if (argc <= 3) {
    fprintf(stderr, "no input file.\n");
    PrintUsage(stderr);
    return 1;
  }

  const char* path = argv[1];
  const char* entry = argv[2];
  const char* file = argv[3];

  // Simply pass allocations through to malloc. We won't be able to track or
  // free memory that the caller is supposed to track and free, but we don't
  // leak memory in a loop and we assume this is the main entry point of the
  // program, so we should be okay.
  FblcArena* arena = &FblcMallocArena;

  FbldQRef* qentry = FbldParseQRefFromString(arena, entry);
  if (qentry == NULL) {
    fprintf(stderr, "failed to parse entry\n");
    return 1;
  }

  FbldAccessLocV accessv;
  FblcVectorInit(arena, accessv);
  FbldLoaded* loaded = FbldLoadCompileProgram(arena, &accessv, path, qentry);
  if (loaded == NULL) {
    return 1;
  }

  FILE* fin = fopen(file, "rb");
  if (fin == NULL) {
    fprintf(stderr, "unable to open %s\n", file);
    return 1;
  }

  IOUser user = { .fin = fin };
  FblcIO io = { .io = &IO, .user = &user };
  FblcInstr instr = { .on_undefined_access = NULL };

  FblcDebugMallocArena debug_arena;
  FblcInitDebugMallocArena(&debug_arena);
  arena = &debug_arena._base;

  FblcValue* value = FblcExecute(arena, &instr, loaded->proc_c, NULL, &io);

  // Print the md5 hash
  char* hex = "0123456789abcdef";
  assert(value->fieldc == 32);
  for (size_t i = 0; i < 32; ++i) {
    int tag = value->fields[i]->tag;
    assert(tag < 16);
    printf("%c", hex[tag]);
  }
  printf("\n");
  FblcRelease(arena, value);
  FblcAssertEmptyDebugMallocArena(&debug_arena);

  return 0;
}
