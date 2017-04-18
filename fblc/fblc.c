// fblc.c --
//   This file implements the main entry point for the fblc interpreter.

#define _POSIX_SOURCE   // for fdopen
#include <assert.h>     // for assert
#include <poll.h>       // for poll
#include <stdio.h>      // for fdopen
#include <stdlib.h>     // for malloc, free
#include <string.h>     // for strcmp
#include <unistd.h>     // for read

#include "fblcs.h"

// IOUser --
//   User data for FblcIO.
typedef struct {
  FblcsProgram* sprog;
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
      "Usage: fblc FILE MAIN [ARG...]\n"
      "Execute the function or process called MAIN in the environment of the\n"
      "fblc program FILE with the given ARGs.\n"  
      "Ports should be provided by arranging for file descriptors 3, 4, ...\n"
      "to be open on which data for port 1, 2, ... can be read or written as\n"
      "appropriate.\n"
      "ARG is a value text representation of the argument value.\n"
      "The number of arguments must match the expected types for the MAIN\n"
      "function or process.\n"
      "Example: fblc program.fblc main 3<in.port 4>out.port 'Bool:true(Unit())'\n"
  );
}

// IO --
//   io function for external ports with IOUser as user data.
//   See the corresponding documentation in fblc.h.
static void IO(void* user, FblcArena* arena, bool block, FblcValue** ports)
{
  IOUser* io_user = (IOUser*)user;

  struct pollfd fds[io_user->proc->portv.size];
  for (size_t i = 0; i < io_user->proc->portv.size; ++i) {
    fds[i].fd = 3 + i;
    fds[i].events = POLLIN;
    fds[i].revents = 0;

    if (io_user->proc->portv.xs[i].polarity == FBLC_PUT_POLARITY) {
      if (ports[i] != NULL) {
        FILE* fout = fdopen(fds[i].fd, "w");
        FblcsPrintValue(fout, io_user->sprog, io_user->proc->portv.xs[i].type, ports[i]);
        fprintf(fout, "\n");
        fflush(fout);
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

  poll(fds, io_user->proc->portv.size, block ? -1 : 0);

  for (size_t i = 0; i < io_user->proc->portv.size; ++i) {
    if (fds[i].revents & POLLIN) {
      assert(io_user->proc->portv.xs[i].polarity == FBLC_GET_POLARITY);
      assert(ports[i] == NULL);
      assert(fds[i].fd >= 0);
      ports[i] = FblcsParseValue(arena, io_user->sprog, io_user->proc->portv.xs[i].type, fds[i].fd);

      // Parse the trailing newline.
      char newline;
      read(fds[i].fd, &newline, 1);
      assert(newline == '\n');
    }
  }
}


// main --
//   The main entry point for the fblc interpreter. Evaluates the MAIN
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
//   given arguments to standard out, or prints an error message to standard
//   error if an error is encountered.
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
    fprintf(stderr, "no input file.\n");
    PrintUsage(stderr);
    return 1;
  }

  if (argc <= 2) {
    fprintf(stderr, "no main entry point provided.\n");
    PrintUsage(stderr);
    return 1;
  }

  const char* filename = argv[1];
  const char* entry = argv[2];
  argv += 3;
  argc -= 3;

  // Simply pass allocations through to malloc. We won't be able to track or
  // free memory that the caller is supposed to track and free, but we don't
  // leak memory in a loop and we assume this is the main entry point of the
  // program, so we should be okay.
  FblcArena* arena = &FblcMallocArena;

  FblcsProgram* sprog = FblcsLoadProgram(arena, filename);
  if (sprog == NULL) {
    return 1;
  }

  FblcDecl* decl = FblcsLookupDecl(sprog, entry);
  if (decl == NULL) {
    fprintf(stderr, "entry %s not found.\n", entry);
    return 1;
  }

  FblcProcDecl* proc = NULL;
  if (decl->tag == FBLC_PROC_DECL) {
    proc = (FblcProcDecl*)decl;
  } else if (decl->tag == FBLC_FUNC_DECL) {
    // Make a proc wrapper for the function.
    FblcFuncDecl* func = (FblcFuncDecl*)decl;
    FblcEvalActn* body = arena->alloc(arena, sizeof(FblcEvalActn));
    body->_base.tag = FBLC_EVAL_ACTN;
    body->arg = func->body;

    proc = arena->alloc(arena, sizeof(FblcProcDecl));
    proc->_base.tag = FBLC_PROC_DECL;
    proc->portv.size = 0;
    proc->portv.xs = NULL;
    proc->argv.size = func->argv.size;
    proc->argv.xs = func->argv.xs;
    proc->return_type = func->return_type;
    proc->body = &body->_base;
  } else {
    fprintf(stderr, "entry %s is not a function or process.\n", entry);
    return 1;
  }

  if (proc->argv.size != argc) {
    fprintf(stderr, "expected %zi args, but %i were provided.\n", proc->argv.size, argc);
    return 1;
  }

  FblcValue* args[argc];
  for (size_t i = 0; i < argc; ++i) {
    args[i] = FblcsParseValueFromString(arena, sprog, proc->argv.xs[i], argv[i]);
  }

  IOUser user = { .sprog = sprog, .proc = proc };
  FblcIO io = { .io = &IO, .user = &user };

  FblcValue* value = FblcExecute(arena, proc, args, &io);
  assert(value != NULL);

  FblcsPrintValue(stdout, sprog, proc->return_type, value);
  fprintf(stdout, "\n");
  fflush(stdout);
  FblcRelease(arena, value);
  return 0;
}
