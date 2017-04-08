// fbld-test.c --
//   This file implements the main entry point for the fbld-test program.

#include <stdio.h>     // FILE, fprintf, stdout, stderr
#include <string.h>    // for strcmp

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

  fprintf(stderr, "TODO: implement fbld-test.\n");
  return 1;
}
