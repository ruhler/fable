
#include "fble-arg-parse.h"

#include <stdio.h>    // for fprintf
#include <string.h>   // for strcmp

#include "fble-vector.h"    // for FbleVectorAppend

// FbleParseBoolArg -- See documentation in fble-arg-parse.h
bool FbleParseBoolArg(const char* name, bool* dest, int* argc, const char*** argv, bool* error)
{
  if (strcmp(name, (*argv)[0]) == 0) {
    *dest = true;
    (*argc)--;
    (*argv)++;
    return true;
  }
  return false;
}

// FbleParseStringArg --  documentation in fble-arg-parse.h
bool FbleParseStringArg(const char* name, const char** dest, int* argc, const char*** argv, bool* error)
{
  if (strcmp(name, (*argv)[0]) == 0) {
    if (*argc < 2) {
      fprintf(stderr, "Error: missing argument to %s option.\n", name);
      *error = true;
      return true;
    }

    if (*dest != NULL) {
      fprintf(stderr, "Error: duplicate %s option.\n", name);
      *error = true;
      return true;
    }

    *dest = (*argv)[1];
    (*argc) -= 2;
    (*argv) += 2;
    return true;
  }
  return false;
}


// FbleParseSearchPathArg -- See documentation in fble-args.h
bool FbleParseSearchPathArg(const char* name, FbleSearchPath* dest, int* argc, const char*** argv, bool* error)
{
  if (strcmp(name, (*argv)[0]) == 0) {
    if (*argc < 2) {
      fprintf(stderr, "Error: missing argument to %s option.\n", name);
      *error = true;
      return true;
    }

    FbleVectorAppend(*dest, (*argv)[1]);

    (*argc) -= 2;
    (*argv) += 2;
    return true;
  }
  return false;
}

// FbleParseInvalidArg --
//   See documentation in fble-arg-parse.h
bool FbleParseInvalidArg(int* argc, const char*** argv, bool* error)
{
  fprintf(stderr," Error: invalid argument: '%s'\n", (*argv)[0]);
  *error = true;
  return true;
}
