// fblc.c --
//   This file implements the main entry point for the fblc interpreter.

#define _POSIX_SOURCE   // for fdopen
#include <assert.h>     // for assert
#include <poll.h>       // for poll
#include <stdio.h>      // for fdopen
#include <string.h>     // for strcmp
#include <unistd.h>     // for read

#include "fblcs.h"
#include "gc.h"

// IOUser --
//   User data for FblcIO.
typedef struct {
  FblcsProgram* sprog;
  FblcProcDecl* proc;
} IOUser;

static void PrintUsage(FILE* stream);
static void PrintValue(FILE* stream, FblcsProgram* sprog, FblcTypeId typeid, FblcValue* value);
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
      "Usage: fblc FILE MAIN [ARG...] \n"
      "Evaluate the function or process called MAIN in the environment of the\n"
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

// PrintValue --
//   Print a value in standard format to the given FILE stream.
//
// Inputs:
//   stream - The stream to print the value to.
//   value - The value to print.
//
// Result:
//   None.
//
// Side effects:
//   The value is printed to the given file stream.
static void PrintValue(FILE* stream, FblcsProgram* sprog, FblcTypeId typeid, FblcValue* value)
{
  FblcTypeDecl* type = (FblcTypeDecl*)sprog->program->declv[typeid];
  if (type->tag == FBLC_STRUCT_DECL) {
    fprintf(stream, "%s(", DeclName(sprog, typeid));
    for (size_t i = 0; i < type->fieldc; ++i) {
      if (i > 0) {
        fprintf(stream, ",");
      }
      PrintValue(stream, sprog, type->fieldv[i], value->fields[i]);
    }
    fprintf(stream, ")");
  } else if (type->tag == FBLC_UNION_DECL) {
    fprintf(stream, "%s:%s(", DeclName(sprog, typeid), FieldName(sprog, typeid, value->tag));
    PrintValue(stream, sprog, type->fieldv[value->tag], value->fields[0]);
    fprintf(stream, ")");
  } else {
    assert(false && "Invalid Kind");
  }
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
        FILE* fout = fdopen(fds[i].fd, "w");
        PrintValue(fout, io_user->sprog, io_user->proc->portv[i].type, ports[i]);
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

  poll(fds, io_user->proc->portc, block ? -1 : 0);

  for (size_t i = 0; i < io_user->proc->portc; ++i) {
    if (fds[i].revents & POLLIN) {
      assert(io_user->proc->portv[i].polarity == FBLC_GET_POLARITY);
      assert(ports[i] == NULL);
      assert(fds[i].fd >= 0);
      ports[i] = FblcsParseValue(arena, io_user->sprog, io_user->proc->portv[i].type, fds[i].fd);

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

  GcInit();
  FblcArena* gc_arena = CreateGcArena();
  FblcArena* bulk_arena = CreateBulkFreeArena(gc_arena);
  FblcsProgram* sprog = FblcsLoadProgram(bulk_arena, filename);
  if (sprog == NULL) {
    FreeBulkFreeArena(bulk_arena);
    FreeGcArena(gc_arena);
    GcFinish();
    return 1;
  }

  FblcDeclId decl_id = FblcsLookupDecl(sprog, entry);
  if (decl_id == FBLC_NULL_ID) {
    fprintf(stderr, "entry %s not found.\n", entry);
    FreeBulkFreeArena(bulk_arena);
    FreeGcArena(gc_arena);
    GcFinish();
    return 1;
  }

  FblcDecl* decl = sprog->program->declv[decl_id];
  FblcArena* bulk_arena_2 = CreateBulkFreeArena(gc_arena);
  FblcProcDecl* proc = NULL;
  if (decl->tag == FBLC_PROC_DECL) {
    proc = (FblcProcDecl*)decl;
  } else if (decl->tag == FBLC_FUNC_DECL) {
    // Make a proc wrapper for the function.
    FblcFuncDecl* func = (FblcFuncDecl*)decl;
    FblcEvalActn* body = bulk_arena_2->alloc(bulk_arena_2, sizeof(FblcEvalActn));
    body->tag = FBLC_EVAL_ACTN;
    body->arg = func->body;

    proc = bulk_arena_2->alloc(bulk_arena_2, sizeof(FblcProcDecl));
    proc->tag = FBLC_PROC_DECL;
    proc->portc = 0;
    proc->portv = NULL;
    proc->argc = func->argc;
    proc->argv = func->argv;
    proc->return_type = func->return_type;
    proc->body = (FblcActn*)body;
  } else {
    fprintf(stderr, "entry %s is not a function or process.\n", entry);
    FreeBulkFreeArena(bulk_arena_2);
    FreeBulkFreeArena(bulk_arena);
    FreeGcArena(gc_arena);
    GcFinish();
    return 1;
  }

  if (proc->argc != argc) {
    fprintf(stderr, "expected %zi args, but %i were provided.\n", proc->argc, argc);
    FreeBulkFreeArena(bulk_arena_2);
    FreeBulkFreeArena(bulk_arena);
    FreeGcArena(gc_arena);
    GcFinish();
    return 1;
  }

  FblcValue* args[argc];
  for (size_t i = 0; i < argc; ++i) {
    args[i] = FblcsParseValueFromString(gc_arena, sprog, proc->argv[i], argv[i]);
  }

  IOUser user = { .sprog = sprog, .proc = proc };
  FblcIO io = { .io = &IO, .user = &user };

  FblcValue* value = FblcExecute(gc_arena, sprog->program, proc, args, &io);
  assert(value != NULL);

  PrintValue(stdout, sprog, proc->return_type, value);
  fprintf(stdout, "\n");
  fflush(stdout);
  FblcRelease(gc_arena, value);

  FreeBulkFreeArena(bulk_arena_2);
  FreeBulkFreeArena(bulk_arena);
  FreeGcArena(gc_arena);
  GcFinish();
  return 0;
}
