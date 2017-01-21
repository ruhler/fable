// fblcbi.c --
//   This file implements the main entry point for the fblc binary interpreter.

#include <assert.h>     // for assert
#include <poll.h>       // for poll
#include <fcntl.h>      // for open
#include <stdio.h>      // for fprintf
#include <stdlib.h>     // for atoi
#include <string.h>     // for strcmp
#include <sys/stat.h>   // for open
#include <sys/types.h>  // for open
#include <unistd.h>     // for STDOUT_FILENO

#include "fblc.h"
#include "gc.h"

// IOUser --
//   User data for FblcIO.
typedef struct {
  FblcProgram* program;
  FblcProcDecl* proc;
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
      "Usage: fblcbi PROGRAM MAIN [ARG...] \n"
      "Evaluate the function or process with id MAIN in the environment of the\n"
      "fblc program PROGRAM with the given ARGs.\n"  
      "PROGRAM should be a file containing a sequence of digits '0' and '1' representing the program.\n"
      "MAIN should be the decimal integer id of the function or process to execute.\n"
      "ARG should be a sequence of digits '0' and '1' representing an argument value.\n"
      "The number of arguments must match the expected number for the MAIN function.\n"
      "Ports should be provided by arranging for file descriptors 3, 4, ...\n"
      "to be open on which data for port 1, 2, ... can be read or written as\n"
      "a sequence of binary digits '0' and '1' as appropriate.\n"
  );
}

// IO --
//   io function for external ports with IOUser as user data.
//   See the corresponding documentation in fblc.h.
static void IO(void* user, FblcArena* arena, bool block, FblcValue** ports)
{
  IOUser* io_user = (IOUser*)user;

  struct pollfd fds[io_user->proc->portc];
  for (size_t i = 0; i < io_user->proc->portc; ++i) {
    fds[i].fd = 3 + i;
    fds[i].events = POLLIN;
    fds[i].revents = 0;

    if (io_user->proc->portv[i].polarity == FBLC_PUT_POLARITY) {
      if (ports[i] != NULL) {
        FblcWriteValue(ports[i], fds[i].fd);
        FblcRelease(arena, ports[i]);
        ports[i] = NULL;
      }
      fds[i].fd = -1;
    } else {
      if (ports[i] != NULL) {
        fds[i].fd = -1;
      }
    }
  }

  poll(fds, io_user->proc->portc, block ? -1 : 0);

  for (size_t i = 0; i < io_user->proc->portc; ++i) {
    if (fds[i].revents & POLLIN) {
      assert(io_user->proc->portv[i].polarity == FBLC_GET_POLARITY);
      assert(ports[i] == NULL);
      assert(fds[i].fd >= 0);
      ports[i] = FblcReadValue(arena, io_user->program, io_user->proc->portv[i].type, fds[i].fd);
    }
  }
}

// main --
//   The main entry point for the fblc binary interpreter. Evaluates the MAIN
//   function or process from the given program with the given ports and
//   arguments. The result is printed to standard output.
//
// Inputs:
//   argc - The number of command line arguments.
//   argv - The command line arguments.
//
// Results:
//   0 on success, non-zero on error.
//
// Side effects:
//   Prints the value resulting from the evaluation of the MAIN process on the
//   given arguments to standard out using the binary text format, or prints
//   an error message to standard error if an error is encountered.
int main(int argc, char* argv[])
{
  if (argc > 1 && strcmp("--help", argv[1]) == 0) {
    PrintUsage(stdout);
    return 0;
  }

  if (argc <= 1) {
    fprintf(stderr, "no input program.\n");
    return 1;
  }

  if (argc <= 2) {
    fprintf(stderr, "no main entry point provided.\n");
    return 1;
  }

  const char* program_file = argv[1];
  size_t entry = atoi(argv[2]);
  argv += 3;
  argc -= 3;

  int fdin = open(program_file, O_RDONLY);
  if (fdin < 0) {
    fprintf(stderr, "Unable to open %s for reading\n", program_file);
    return 1;
  }

  GcInit();
  FblcArena* program_underlying_arena = CreateGcArena();
  FblcArena* program_arena = CreateBulkFreeArena(program_underlying_arena);
  FblcProgram* program = FblcReadProgram(program_arena, fdin);
  if (entry >= program->declc) {
    fprintf(stderr, "invalid entry id: %zi.\n", entry);
    FreeBulkFreeArena(program_arena);
    FreeGcArena(program_underlying_arena);
    GcFinish();
    return 1;
  }

  FblcDecl* decl = program->declv[entry];
  FblcProcDecl* proc = NULL;
  if (decl->tag == FBLC_PROC_DECL) {
    proc = (FblcProcDecl*)decl;
  } else if (decl->tag == FBLC_FUNC_DECL) {
    // Make a proc wrapper for the function.
    FblcFuncDecl* func = (FblcFuncDecl*)decl;
    FblcEvalActn* body = program_arena->alloc(program_arena, sizeof(FblcEvalActn));
    body->tag = FBLC_EVAL_ACTN;
    body->arg = func->body;

    proc = program_arena->alloc(program_arena, sizeof(FblcProcDecl));
    proc->tag = FBLC_PROC_DECL;
    proc->portc = 0;
    proc->portv = NULL;
    proc->argc = func->argc;
    proc->argv = func->argv;
    proc->return_type = func->return_type;
    proc->body = (FblcActn*)body;
  } else {
    fprintf(stderr, "entry %zi is not a function or process.\n", entry);
    FreeBulkFreeArena(program_arena);
    FreeGcArena(program_underlying_arena);
    GcFinish();
    return 1;
  }

  if (proc->argc != argc) {
    fprintf(stderr, "expected %zi args, but %i were provided.\n", proc->argc, argc);
    FreeBulkFreeArena(program_arena);
    FreeGcArena(program_underlying_arena);
    GcFinish();
    return 1;
  }

  FblcArena* exec_arena = CreateGcArena();
  FblcValue* args[argc];
  for (size_t i = 0; i < argc; ++i) {
    args[i] = FblcReadValueFromString(exec_arena, program, proc->argv[i], argv[i]);
  }

  IOUser user = { .program = program, .proc = proc };
  FblcIO io = { .io = &IO, .user = &user };

  FblcValue* value = FblcExecute(exec_arena, program, proc, args, &io);
  assert(value != NULL);

  FblcWriteValue(value, STDOUT_FILENO);
  FblcRelease(exec_arena, value);

  FreeGcArena(exec_arena);
  FreeBulkFreeArena(program_arena);
  FreeGcArena(program_underlying_arena);
  GcFinish();
  return 0;
}
