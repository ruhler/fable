/**
 * @file arg-parse.c
 * Implementation of arg parse routines.
 */

#include <fble/fble-arg-parse.h>

#include <stdio.h>    // for fprintf
#include <string.h>   // for strcmp

#include <fble/fble-load.h>      // for FbleSearchPath, etc.

// FbleParseBoolArg -- See documentation in fble-arg-parse.h
bool FbleParseBoolArg(const char* name, bool* dest, int* argc, const char*** argv, bool* error)
{
  (void)error;

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
bool FbleParseSearchPathArg(FbleSearchPath* dest, int* argc, const char*** argv, bool* error)
{
  if (strcmp("-I", (*argv)[0]) == 0) {
    if (*argc < 2) {
      fprintf(stderr, "Error: missing argument to -I option.\n");
      *error = true;
      return true;
    }

    FbleSearchPathAppend(dest, (*argv)[1]);

    (*argc) -= 2;
    (*argv) += 2;
    return true;
  }

  if ((*argv)[0][0] == '-' && (*argv)[0][1] == 'I') {
    FbleSearchPathAppend(dest, (*argv[0]) + 2);
    (*argc)--;
    (*argv)++;
    return true;
  }

  const char* package = NULL;
  if (FbleParseStringArg("--package", &package, argc, argv, error)
      || FbleParseStringArg("-p", &package, argc, argv, error)) {
    if (!(*error)) {
      FbleString* path = FbleFindPackage(package);
      if (path != NULL) {
        FbleSearchPathAppendString(dest, path);
        FbleFreeString(path);
      } else {
        fprintf(stderr, "Error: package '%s' not found\n", package);
        *error = true;
      }
    }
    return true;
  }

  return false;
}

// FbleParseInvalidArg --
//   See documentation in fble-arg-parse.h
bool FbleParseInvalidArg(int* argc, const char*** argv, bool* error)
{
  (void)argc;

  fprintf(stderr, "Error: invalid argument: '%s'\n", (*argv)[0]);
  *error = true;
  return true;
}
