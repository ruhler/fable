// fblc-test.c --
//   This file implements the main entry point for the fblc-test program.

#define _GNU_SOURCE   // for getline
#include <assert.h>   // for assert
#include <stdio.h>    // for getline
#include <stdlib.h>   // for abort
#include <string.h>   // for strcmp

#include "fblcs.h"

// CmdTag --
//   Enum used to distinguish among the different command types.
typedef enum {
  CMD_PUT, CMD_GET, CMD_RETURN
} CmdTag;

// Command --
//   Structure representing a command to execute.
typedef struct {
  CmdTag tag;
  FblcFieldId port;
  FblcValue* value;
} Command;

// IOUser --
//   User data for FblcIO.
//
// Fields:
//   sprog - The program environment.
//   proc - The main entry.
//   file - The name of the file containing the commands to execute.
//   line - The line number of the current command being executed.
//   stream - The stream with the commands to be executed.
//   cmd - The next command to execute, if cmd_ready is true.
typedef struct {
  FblcsProgram* sprog;
  FblcProcDecl* proc;
  const char* file;
  size_t line;
  FILE* stream;
  bool cmd_ready;
  Command cmd;
} IOUser;

static void PrintUsage(FILE* stream);
static void ReportError(IOUser* user, const char* msg);
static void EnsureCommandReady(IOUser* user, FblcArena* arena);
static bool ValuesEqual(FblcValue* a, FblcValue* b);
static void AssertValuesEqual(IOUser* user, FblcTypeDecl* type, FblcValue* a, FblcValue* b);
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
      "Usage: fblc-test SCRIPT FILE MAIN [ARG...]\n"
      "Execute the function or process called MAIN in the environment of the\n"
      "fblc program FILE with the given ARGs.\n"  
      "The program is driven and tested based on the sequence of commands\n"
      "read from SCRIPT. The commands are of the form:"
      "      put NAME VALUE\n"
      "or    get NAME VALUE\n"
      "or    return VALUE\n"
      "The put command puts the fblc text VALUE onto the named port.\n"
      "The get command reads the fblc value from the named port and asserts\n"
      "that the value read matches the given value.\n" 
      "The return command waits for the result of the process and asserts\n"
      "that the resultinv value matches the given value.\n"
  );
}

// ReportError --
//   Reports an error message with command location to stderr.
//
// Inputs:
//   user - Contains the current location of the command script.
//   msg - An error message to print.
//
// Results:
//   None.
//
// Side effects:
//   Outputs the current command location and error message to stderr.
static void ReportError(IOUser* user, const char* msg)
{
  fprintf(stderr, "%s:%zd: error: %s", user->file, user->line, msg);
}

// EnsureCommandReady --
//   Read the next command to execute if the current command is not ready.
//
// Inputs:
//   arena - Arena used to allocated fblc values used in the commands.
//   user - Contains the command input stream and cmd result.
//
// Results:
//   None.
//
// Side effects:
//   Reads the next command from the command stream and sets user->cmd_ready
//   to true. Aborts if there is no next command or the command is malformed.
static void EnsureCommandReady(IOUser* user, FblcArena* arena)
{
  if (!user->cmd_ready) {
    char* line = NULL;
    size_t len = 0;
    if (getline(&line, &len, user->stream) < 0) {
      ReportError(user, "failed to read command\n");
      abort();
    }
    user->line++;

    FblcTypeDecl* type = NULL;
    char port[len+1];
    char value[len+1];
    if (sscanf(line, "get %s %s", port, value) == 2) {
      user->cmd.tag = CMD_GET;
      user->cmd.port = FblcsLookupPort(user->sprog, user->proc, port);
      if (user->cmd.port == FBLC_NULL_ID) {
        ReportError(user, "port not defined: ");
        fprintf(stderr, "'%s'\n", port);
        abort();
      }
      if (user->proc->portv.xs[user->cmd.port].polarity != FBLC_PUT_POLARITY) {
        ReportError(user, "expected put polarity for port ");
        fprintf(stderr, "'%s'\n", port);
        abort();
      }
      type = user->proc->portv.xs[user->cmd.port].type;
    } else if (sscanf(line, "put %s %s", port, value) == 2) {
      user->cmd.tag = CMD_PUT;
      user->cmd.port = FblcsLookupPort(user->sprog, user->proc, port);
      if (user->cmd.port == FBLC_NULL_ID) {
        ReportError(user, "port not defined: ");
        fprintf(stderr, "'%s'\n", port);
        abort();
      }
      if (user->proc->portv.xs[user->cmd.port].polarity != FBLC_GET_POLARITY) {
        ReportError(user, "expected get polarity for port ");
        fprintf(stderr, "'%s'\n", port);
        abort();
      }
      type = user->proc->portv.xs[user->cmd.port].type;
    } else if (sscanf(line, "return %s", value) == 1) {
      user->cmd.tag = CMD_RETURN;
      type = user->proc->return_type;
    } else {
      ReportError(user, "malformed command line: ");
      fprintf(stderr, "'%s'\n", line);
      abort();
    }

    user->cmd.value = FblcsParseValueFromString(arena, user->sprog, type, value);
    if (user->cmd.value == NULL) {
      ReportError(user, "error parsing value\n");
      abort();
    }
    user->cmd_ready = true;
    free(line);
  }
}

// ValuesEqual --
//   Check whether two values are structurally equal.
//
// Inputs:
//   a - the first value. 
//   b - the second value. 
//
// Result:
//   true if the first value is structurally equivalent to the second value,
//   false otherwise.
//
// Side effects:
//   None.
static bool ValuesEqual(FblcValue* a, FblcValue* b)
{
  if (a->kind != b->kind) {
    return false;
  }

  if (a->fieldc != b->fieldc) {
    return false;
  }

  if (a->tag != b->tag) {
    return false;
  }

  size_t fieldc = a->fieldc;
  if (a->kind == FBLC_UNION_KIND) {
    fieldc = 1;
  }

  for (size_t i = 0; i < fieldc; ++i) {
    if (!ValuesEqual(a->fields[i], b->fields[i])) {
      return false;
    }
  }

  return true;
}

// AssertValuesEqual --
//   Assert that the two given values are equal.
//
// Input:
//   user - Used for error reporting purposes.
//   type - The type of value being compared.
//   a - The first value.
//   b - The second value.
//
// Results:
//   None.
//
// Side effects:
//   Reports an error message to stderr and aborts if the two values are not
//   structurally equivalent.
static void AssertValuesEqual(IOUser* user, FblcTypeDecl* type, FblcValue* a, FblcValue* b)
{
  if (!ValuesEqual(a, b)) {
    ReportError(user, "value mismatch.");
    fprintf(stderr, "\nexpected: ");
    FblcsPrintValue(stderr, user->sprog, type, a);
    fprintf(stderr, "\nactual  : ");
    FblcsPrintValue(stderr, user->sprog, type, b);
    fprintf(stderr, "\n");
    abort();
  }
}

// IO --
//   io function for external ports with IOUser as user data.
//   See the corresponding documentation in fblc.h.
static void IO(void* user, FblcArena* arena, bool block, FblcValue** ports)
{
  IOUser* io_user = (IOUser*)user;
  EnsureCommandReady(io_user, arena);
  if (io_user->cmd.tag == CMD_GET && ports[io_user->cmd.port] != NULL) {
    AssertValuesEqual(io_user, io_user->proc->portv.xs[io_user->cmd.port].type, io_user->cmd.value, ports[io_user->cmd.port]);
    io_user->cmd_ready = false;
    FblcRelease(arena, ports[io_user->cmd.port]);
    ports[io_user->cmd.port] = NULL;
  } else if (io_user->cmd.tag == CMD_PUT && ports[io_user->cmd.port] == NULL) {
    io_user->cmd_ready = false;
    ports[io_user->cmd.port] = io_user->cmd.value;
  } else if (block) {
    ReportError(io_user, "process blocked\n");
    abort();
  }
}

// main --
//   The main entry point for the fblc-test program.
//
// Inputs:
//   argc - The number of command line arguments.
//   argv - The command line arguments.
//
// Results:
//   0 on success, non-zero on error.
//
// Side effects:
//   Reads and executes test commands from stdin.
//   Prints an error to stderr and exits the program in the case of error.
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
    fprintf(stderr, "no script file.\n");
    PrintUsage(stderr);
    return 1;
  }

  if (argc <= 2) {
    fprintf(stderr, "no input file.\n");
    PrintUsage(stderr);
    return 1;
  }

  if (argc <= 3) {
    fprintf(stderr, "no main entry point provided.\n");
    PrintUsage(stderr);
    return 1;
  }

  const char* script = argv[1];
  const char* filename = argv[2];
  const char* entry = argv[3];
  argv += 4;
  argc -= 4;

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

  IOUser user;
  user.sprog = sprog;
  user.proc = proc;
  user.file = script;
  user.line = 0;
  user.stream = fopen(script, "r");
  if (user.stream == NULL) {
    fprintf(stderr, "failed to open command script '%s'\n", script);
    return 1;
  }
  user.cmd_ready = false;
  FblcIO io = { .io = &IO, .user = &user };
  FblcInstr instr = { .on_undefined_access = NULL };

  FblcValue* value = FblcExecute(arena, &instr, proc, args, &io);
  assert(value != NULL);

  EnsureCommandReady(&user, arena);
  if (user.cmd.tag != CMD_RETURN) {
    ReportError(&user, "premature program termination.\n");
    abort();
  }
  AssertValuesEqual(&user, proc->return_type, user.cmd.value, value);
  FblcRelease(arena, user.cmd.value);
  FblcRelease(arena, value);
  return 0;
}
