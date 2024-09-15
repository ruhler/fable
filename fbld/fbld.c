/**
 * @file fbld.c
 *  The main entry point for the fbld program.
 */

#include "fbld.h"

#include <stdbool.h>    // for bool
#include <string.h>     // for strcmp

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

  bool debug = false;
  if (argc > 0 && strcmp(*argv, "--debug") == 0) {
    debug = true;
    argc--;
    argv++;
  }

  FbldMarkup* parsed = FbldParse(argv);
  if (parsed == NULL) {
    return 1;
  }

  FbldMarkup* evaled = FbldEval(parsed, debug);
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
