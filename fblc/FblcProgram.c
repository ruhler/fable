// FblcProgram.c --
//
//   The file contains utilities for working with the abstract syntax for Fblc
//   programs.

#include "FblcInternal.h"

struct FblcLoc {
  const char* source;
  int line;
  int col;
};

typedef struct TypeEnv {
  FblcType* decl;
  struct TypeEnv* next;
} TypeEnv;

typedef struct FuncEnv {
  FblcFunc* decl;
  struct FuncEnv* next;
} FuncEnv;

struct FblcEnv {
  TypeEnv* types;
  FuncEnv* funcs;
};

// FblcNamesEqual --
//
//   Test whether two names are the same.
//
// Inputs:
//   a - The first name for the comparison.
//   b - The second name for the comparison.
//
// Result:
//   The value true if the names 'a' and 'b' are the same, false otherwise.
//
// Side effects:
//   None

bool FblcNamesEqual(FblcName a, FblcName b)
{
  return strcmp(a, b) == 0;
}

// FblcNewLoc --
//
//   Create a new location object. Location objects are used to identify a
//   file, line number, and column number that is the source of a token,
//   expression, or other part of the abstract syntax. Location objects are
//   used to provide location information in error message.
//
// Inputs:
//   source - The filename or some other description of the source of a line
//            of code.
//   line - The line number of the location. The first line is 1.
//   col - The column number of the location. The first column is 1.
//
// Results:
//   The created location object.
//
// Side effects:
//   None.

FblcLoc* FblcNewLoc(const char* source, int line, int col)
{
  FblcLoc* loc = GC_MALLOC(sizeof(FblcLoc));
  loc->source = source;
  loc->line = line;
  loc->col = col;
  return loc;
}

// FblcReportError --
//
//   Prints a formatted error message to standard error with location
//   information. The format is the same as for printf, with the first
//   argument for conversion following the loc argument.
//   
// Inputs:
//   format - A printf style format string.
//   loc - The location associated with the error.
//   ... - Subsequent arguments for conversion based on the format string.
//
// Result:
//   None.
//
// Side effects:
//   Prints an error message to standard error.

void FblcReportError(const char* format, const FblcLoc* loc, ...)
{
  va_list ap;
  va_start(ap, loc);
  fprintf(stderr, "%s:%d:%d: error: ", loc->source, loc->line, loc->col);
  vfprintf(stderr, format, ap);
  va_end(ap);
}

// FblcNewEnv --
//
//   Create a new, empty, Fblc environment.
//
// Inputs:
//   None.
//
// Result:
//   A new, empty, Fblc environment.
//
// Side effects:
//   None.

FblcEnv* FblcNewEnv()
{
  return GC_MALLOC(sizeof(FblcEnv));
}

// FblcLookupType --
//
//   Look up the declaration of the type with the given name in the given
//   environment.
// 
// Inputs:
//   env - The environment to look up the type in.
//   name - The name of the type to look up.
//
// Result:
//   The declaration for the type with the given name, or NULL if there is no
//   type with the given name in the given environment.
//
// Side effects:
//   None.

FblcType* FblcLookupType(const FblcEnv* env, FblcName name)
{
  for (TypeEnv* tenv = env->types; tenv != NULL; tenv = tenv->next) {
    if (FblcNamesEqual(tenv->decl->name.name, name)) {
      return tenv->decl;
    }
  }
  return NULL;
}

// FblcLookupFunc --
//
//   Look up the declaration of the function with the given name in the given
//   environment.
// 
// Inputs:
//   env - The environment to look up the type in.
//   name - The name of the function to look up.
//
// Result:
//   The declaration for the function with the given name, or NULL if there is
//   no function with the given name in the given environment.
//
// Side effects:
//   None.
FblcFunc* FblcLookupFunc(const FblcEnv* env, FblcName name)
{
  for (FuncEnv* fenv = env->funcs; fenv != NULL; fenv = fenv->next) {
    if (FblcNamesEqual(fenv->decl->name.name, name)) {
      return fenv->decl;
    }
  }
  return NULL;
}

// FblcAddType --
//  
//   Add a type declaration to the given environment.
//
// Inputs:
//   env - The environment to add the type declaration to.
//   type - The type declaration to add.
//
// Result:
//   True if the type was successfully added, false otherwise.
//
// Side effects:
//   The type declaration is added to the environment. If the type declaration
//   could not be added to the environment, an error message is printed to
//   standard error explaining the problem.

bool FblcAddType(FblcEnv* env, FblcType* type)
{
  if (FblcLookupType(env, type->name.name) != NULL
      || FblcLookupFunc(env, type->name.name) != NULL) {
    FblcReportError("Multiple declarations for %s.\n",
       type->name.loc, type->name.name);
    return false;
  }

  TypeEnv* types = GC_MALLOC(sizeof(TypeEnv));
  types->decl = type;
  types->next = env->types;
  env->types = types;
  return true;
}

// FblcAddFunc --
//  
//   Add a function declaration to the given environment.
//
// Inputs:
//   env - The environment to add the function declaration to.
//   func - The function declaration to add.
//
// Result:
//   True if the function was successfully added, false otherwise.
//
// Side effects:
//   The function declaration is added to the environment. If the function
//   could not be added to the environment, an error message is printed to
//   standard error describing the problem.

bool FblcAddFunc(FblcEnv* env, FblcFunc* func)
{
  if (FblcLookupType(env, func->name.name) != NULL
      || FblcLookupFunc(env, func->name.name) != NULL) {
    FblcReportError("Multiple declarations for %s.\n",
       func->name.loc, func->name.name);
    return false;
  }
  FuncEnv* funcs = GC_MALLOC(sizeof(FuncEnv));
  funcs->decl = func;
  funcs->next = env->funcs;
  env->funcs = funcs;
  return true;
}
