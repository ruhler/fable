/**
 * @file fbld.c
 *  The main entry point for the fbld program.
 */

#include "fbld.h"

#include <stdbool.h>    // for bool
#include <stdio.h>      // for printf
#include <string.h>     // for strcmp

/**
 * @value[FbldBuildStamp] A string describing the particular build of fbld.
 *  The string includes information about the version and particular commit
 *  fble was built from. For debug purposes only.
 *
 *  @type[const char*]
 */
extern const char* FbldBuildStamp;

/**
 * @func[PrintUsage] Prints usage to stdout.
 *  @sideeffects Prints usage to stdout.
 */
static void PrintUsage()
{
  printf("fble - fbld document processor\n\n");
  printf("Usage: fbld [OPTION...] [FILE]...\n\n");
  printf("Concatenates the given files together and processes the result as an fbld\n");
  printf("document. The result is output to standard output.\n\n");
  printf("See the fbld language specification for more information about fbld document\n");
  printf("processing.\n\n");
  printf("Generic Program Information:\n");
  printf("  -h, --help\n");
  printf("      display this help text and exit\n");
  printf("  -v, --version\n");
  printf("      display version information and exit\n\n");
  printf("Exit Status:\n");
  printf("  0 Success.\n");
  printf("  1 An error occured when processing the files.\n\n");
  printf("Examples:\n");
  printf("  fble html.fbld foo.fbld > foo.html\n\n");
  printf("  Processes the document foo.fbld as an html document and outputs the result to foo.html.\n");
}

/**
 * @func[main] The main entry point for the fbld program.
 *  @arg[int][argc] The number of command line arguments.
 *  @arg[const char**][argv] The command line arguments.
 *
 *  @returns[int] 0 on success, non-zero on error.
 *
 *  @sideeffects
 *   Prints an error to stderr and exits the program in the case of error.
 */
int main(int argc, const char* argv[])
{
  argc--;
  argv++;

  // Deal with --help, --version command line options.
  for (size_t i = 0; i < argc; ++i) {
    if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      PrintUsage();
      return 0;
    }

    if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
      printf("fbld %s\n", FbldBuildStamp);
      return 0;
    }
  }

  FbldMarkup* parsed = FbldParse(argv);
  if (parsed == NULL) {
    return 1;
  }

  FbldMarkup* evaled = FbldEval(parsed);
  FbldFreeMarkup(parsed);
  if (evaled == NULL) {
    return 1;
  }

  if (!FbldPrintMarkup(evaled)) {
    FbldFreeMarkup(evaled);
    return 1;
  }

  FbldFreeMarkup(evaled);
  return 0;
}
