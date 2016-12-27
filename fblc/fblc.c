// fblc.c --
//   This file implements the main entry point for the fblc interpreter.

#define _POSIX_SOURCE   // for fdopen
#include <assert.h>     // for assert
#include <poll.h>       // for poll
#include <stdio.h>      // for fdopen
#include <string.h>     // for strcmp

#include "fblct.h"
#include "gc.h"

// PortData --
//   User data for IOPorts.
typedef struct {
  FblcArena* arena;
  Env* program;
  FblcTypeId type;
  int fd;
} PortData;

static void PrintUsage(FILE* stream);
static void PrintValue(FILE* stream, Env* env, FblcTypeId typeid, FblcValue* value);
static FblcValue* GetIO(void* data, FblcValue* value);
static FblcValue* PutIO(void* data, FblcValue* value);
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
      "Example: fblc main 3<in.port 4>out.port 'Bool:true(Unit())'\n"
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
static void PrintValue(FILE* stream, Env* env, FblcTypeId typeid, FblcValue* value)
{
  TypeDecl* type = (TypeDecl*)env->declv[typeid];
  if (type->tag == STRUCT_DECL) {
    fprintf(stream, "%s(", type->name.name);
    for (int i = 0; i < type->fieldc; i++) {
      if (i > 0) {
        fprintf(stream, ",");
      }
      PrintValue(stream, env, type->fieldv[i].type.id, value->fields[i]);
    }
    fprintf(stream, ")");
  } else if (type->tag == UNION_DECL) {
    fprintf(stream, "%s:%s(", type->name.name, type->fieldv[value->tag].name.name);
    PrintValue(stream, env, type->fieldv[value->tag].type.id, value->fields[0]);
    fprintf(stream, ")");
  } else {
    assert(false && "Invalid Kind");
  }
}

// GetIO --
//   io function for get ports. See the corresponding documentation in fblc.h.
//
// Inputs:
//   data - PortData with the arena, program, type, and fd for reading the
//          next value.
//   value - ignored.
//
// Results:
//   The next value of the given type read from the given file, or NULL if no
//   value is available.
//
// Side effects:
//   Performs arena allocations.
//   Reads a value from the given file.
static FblcValue* GetIO(void* data, FblcValue* value)
{
  PortData* port_data = (PortData*)data;

  struct pollfd fds;
  fds.fd = port_data->fd;
  fds.events = POLLIN;
  fds.revents = 0;
  poll(&fds, 1, 0);
  if (fds.revents & POLLIN) {
    return ParseValue(port_data->arena, port_data->program, port_data->type, port_data->fd);
  }
  return NULL;
}

// PutIO --
//   io function for put ports. See the corresponding documentation in fblc.h.
//
// Inputs:
//   data - PortData with the fd for writing the next value.
//   value - The value to write.
//
// Results:
//   NULL.
//
// Side effects:
//   Writes the given value to the given file.
static FblcValue* PutIO(void* data, FblcValue* value)
{
  PortData* port_data = (PortData*)data;
  FILE* fout = fdopen(port_data->fd, "w");
  PrintValue(fout, port_data->program, port_data->type, value);
  fprintf(fout, "\n");
  fflush(fout);
  return NULL;
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
    return 1;
  }

  if (argc <= 2) {
    fprintf(stderr, "no main entry point provided.\n");
    return 1;
  }

  const char* filename = argv[1];
  const char* entry = argv[2];
  argv += 3;
  argc -= 3;

  GcInit();
  FblcArena* gc_arena = CreateGcArena();
  FblcArena* bulk_arena = CreateBulkFreeArena(gc_arena);
  Env* env = ParseProgram(bulk_arena, filename);
  if (env == NULL) {
    fprintf(stderr, "failed to parse input FILE.\n");
    FreeBulkFreeArena(bulk_arena);
    FreeGcArena(gc_arena);
    GcFinish();
    return 1;
  }

  if (!CheckProgram(env)) {
    fprintf(stderr, "input FILE is not a well formed  program.\n");
    FreeBulkFreeArena(bulk_arena);
    FreeGcArena(gc_arena);
    GcFinish();
    return 1;
  }

  FblcArena* bulk_arena_2 = CreateBulkFreeArena(gc_arena);
  FblcProgram* program = StripProgram(bulk_arena_2, env);

  FblcDecl* decl = NULL;
  for (size_t i = 0; i < env->declc; ++i) {
    if (NamesEqual(env->declv[i]->name.name, entry)) {
      decl = program->declv[i];
      break;
    }
  }

  if (decl == NULL) {
    fprintf(stderr, "entry %s not found.\n", entry);
    FreeBulkFreeArena(bulk_arena_2);
    FreeBulkFreeArena(bulk_arena);
    FreeGcArena(gc_arena);
    GcFinish();
    return 1;
  }

  FblcProcDecl* proc = NULL;
  if (decl->tag == FBLC_PROC_DECL) {
    proc = (FblcProcDecl*)decl;
  } else if (decl->tag == FBLC_FUNC_DECL) {
    // Make a proc wrapper for the function.
    FblcFuncDecl* func = (FblcFuncDecl*)decl;
    FblcEvalActn* body = bulk_arena_2->alloc(bulk_arena_2, sizeof(FblcEvalActn));
    body->tag = FBLC_EVAL_ACTN;
    body->expr = func->body;

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
    args[i] = ParseValueFromString(gc_arena, env, proc->argv[i], argv[i]);
  }

  FblcIOPort ports[proc->portc];
  PortData port_data[proc->portc];
  for (size_t i = 0; i < proc->portc; ++i) {
    ports[i].io = proc->portv[i].polarity == FBLC_PUT_POLARITY ? &PutIO : &GetIO;
    ports[i].data = port_data + i;
    port_data[i].arena = gc_arena;
    port_data[i].program = env;
    port_data[i].type = proc->portv[i].type;
    port_data[i].fd = 3+i;
  }

  FblcValue* value = FblcExecute(gc_arena, program, proc, args, ports);
  assert(value != NULL);

  PrintValue(stdout, env, proc->return_type, value);
  fprintf(stdout, "\n");
  fflush(stdout);
  FblcRelease(gc_arena, value);

  FreeBulkFreeArena(bulk_arena_2);
  FreeBulkFreeArena(bulk_arena);
  FreeGcArena(gc_arena);
  GcFinish();
  return 0;
}
