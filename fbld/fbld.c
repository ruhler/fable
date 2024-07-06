/**
 * @file fbld.c
 *  The main entry point for the fbld program.
 */

#include "fbld.h"

#define EX_SUCCESS 0
#define EX_FAIL 1

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
  FbldMarkup* parsed = FbldParse(argc, argv);
  if (parsed == NULL) {
    return EX_FAIL;
  }

  FbldMarkup* evaled = FbldEval(parsed);
  FbldFreeMarkup(parsed);
  if (evaled == NULL) {
    return EX_FAIL;
  }

  bool printed = FbldPrintMarkup(evaled);
  FbldFreeMarkup(evaled);
  if (!printed) {
    return EX_FAIL;
  }

  return EX_SUCCESS;
}
