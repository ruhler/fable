/**
 * @file fbld.c
 *  The main entry point for the fbld program.
 */

#include "fbld.h"

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
  FbldMarkup* parsed = FbldParse(argv + 1);
  FbldMarkup* evaled = FbldEval(parsed);
  FbldPrintMarkup(evaled);

  FbldFreeMarkup(parsed);
  FbldFreeMarkup(evaled);
  return 0;
}
