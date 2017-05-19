// fbld-test.c --
//   This file implements the main entry point for the fbld-test program.

#include <stdio.h>        // FILE, fprintf, stdout, stderr
#include <string.h>       // for strcmp, strchr, strncpy
#include <unistd.h>       // for access

#include "fblc.h"
#include "fbld.h"

static void PrintUsage(FILE* stream);
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
      "Usage: fbld-test SCRIPT PATH MAIN [ARG...]\n"
      "Execute the function or process called MAIN in the environment of the\n"
      "fbld modules located on the given search PATH with the given ARGs.\n"  
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
      "PATH should be a colon separated list of directories to search for fbld\n"
      "modules.\n"
      "MAIN should be a qualified entry, such as Foo@main.\n"
      "VALUEs should be specified using qualified names.\n"
  );
}

// main --
//   The main entry point for the fbld-test program.
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
    fprintf(stderr, "no module search path.\n");
    PrintUsage(stderr);
    return 1;
  }

  if (argc <= 3) {
    fprintf(stderr, "no main entry point provided.\n");
    PrintUsage(stderr);
    return 1;
  }

  const char* script = argv[1];
  const char* path = argv[2];
  const char* entry = argv[3];
  argv += 4;
  argc -= 4;

  // Simply pass allocations through to malloc. We won't be able to track or
  // free memory that the caller is supposed to track and free, but we don't
  // leak memory in a loop and we assume this is the main entry point of the
  // program, so we should be okay.
  FblcArena* arena = &FblcMallocArena;

  FbldStringV search_path;
  FblcVectorInit(arena, search_path);
  FblcVectorAppend(arena, search_path, path);

  // Parse the entry.
  const char* entry_at = strchr(entry, '@');
  if (entry_at == NULL) {
    fprintf(stderr, "entry should be a qualified name of the form 'module@process', but got '%s'\n", entry);
    return 1;
  }
  char entry_module_name[entry_at - entry + 1];
  strncpy(entry_module_name, entry, entry_at - entry);
  entry_module_name[entry_at - entry] = '\0';
  FbldLoc entry_module_loc = {
    .source = entry,
    .line = 1,
    .col = 1
  };
  FbldNameL entry_module = {
    .name = entry_module_name,
    .loc = &entry_module_loc
  };
  FbldLoc entry_name_loc = {
    .source = entry,
    .line = 1,
    .col = entry_at - entry + 2
  };
  FbldNameL entry_name = {
    .name = entry_at + 1,
    .loc = &entry_name_loc
  };
  FbldQualifiedName entry_entity = {
    .module = &entry_module,
    .name = &entry_name
  };

  FbldMDeclV mdeclv;
  FblcVectorInit(arena, mdeclv);

  FbldMDefnV mdefnv;
  FblcVectorInit(arena, mdefnv);

  if (!FbldLoadModules(arena, &search_path, entry_module.name, &mdeclv, &mdefnv)) {
    fprintf(stderr, "failed to load modules");
    return 1;
  }

  FblcDecl* decl = FbldCompile(arena, &mdefnv, &entry_entity);
  if (decl == NULL) {
    fprintf(stderr, "failed to compile");
    return 1;
  }

  fprintf(stderr, "script: %s\n", script);
  fprintf(stderr, "TODO: finish implementing fbld-test.\n");
  return 1;
}
