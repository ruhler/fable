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

  const char* entry_at = strchr(entry, '@');
  if (entry_at == NULL) {
    fprintf(stderr, "entry should be a qualified name of the form 'module@process', but got '%s'\n", entry);
    return 1;
  }

  char entry_module[entry_at - entry + 1];
  strncpy(entry_module, entry, entry_at - entry);
  entry_module[entry_at - entry] = '\0';

  // Simply pass allocations through to malloc. We won't be able to track or
  // free memory that the caller is supposed to track and free, but we don't
  // leak memory in a loop and we assume this is the main entry point of the
  // program, so we should be okay.
  FblcArena* arena = &FblcMallocArena;

  FbldStringV search_path;
  FblcVectorInit(arena, search_path);
  FblcVectorAppend(arena, search_path, path);

  FbldMDeclV mdeclv;
  FblcVectorInit(arena, mdeclv);

  FbldMDecl* mdecl = FbldLoadMDecl(arena, &search_path, entry_module, &mdeclv);
  if (mdecl == NULL) {
    fprintf(stderr, "failed to load mdecl for %s\n", entry_module);
    return 1;
  }


  char mdefn_path[strlen(path) + 1 + strlen(entry_module) + strlen(".mdefn") + 1];
  sprintf(mdefn_path, "%s/%s.mdefn", path, entry_module);
  if (access(mdefn_path, F_OK) < 0) {
    fprintf(stderr, "unable to access %s\n", mdefn_path);
    return 1;
  }

  FbldMDefn* mdefn = FbldParseMDefn(arena, mdefn_path);
  if (mdefn == NULL) {
    fprintf(stderr, "failed to parse mdefn at %s\n", mdefn_path);
    return 1;
  }

  fprintf(stderr, "script: %s\n", script);
  fprintf(stderr, "path: %s\n", path);
  fprintf(stderr, "entry: %s\n", entry);
  fprintf(stderr, "entry_module: %s\n", entry_module);
  fprintf(stderr, "mdefn_path: %s\n", mdefn_path);
  fprintf(stderr, "TODO: finish implementing fbld-test.\n");
  return 1;
}
