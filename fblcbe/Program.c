// Program.c --
//
//   The file contains utilities for working with the abstract syntax for 
//   programs.

#include "Internal.h"


// NamesEqual --
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

bool NamesEqual(Name a, Name b)
{
  return strcmp(a, b) == 0;
}

// ReportError --
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

void ReportError(const char* format, Loc* loc, ...)
{
  va_list ap;
  va_start(ap, loc);
  fprintf(stderr, "%s:%d:%d: error: ", loc->source, loc->line, loc->col);
  vfprintf(stderr, format, ap);
  va_end(ap);
}

// NewEnv --
//
//   Create a new environment with the given declarations.
//
// Inputs:
//   alloc - The allocator to use to allocate the environment.
//   declc - The number of declarations of the environment.
//   declv - The declarations of the environment.
//
// Result:
//   A new environment with the given declarations.
//
// Side effects:
//   Allocations are performed using the allocator as necessary to allocate
//   the environment.

Env* NewEnv(Allocator* alloc, int declc, Decl** declv)
{
  Env* env = Alloc(alloc, sizeof(Env));
  env->declc = declc;
  env->declv = declv;
  return env;
}

// LookupType --
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

TypeDecl* LookupType(Env* env, Name name)
{
  for (size_t i = 0; i < env->declc; ++i) {
    Decl* decl = env->declv[i];
    if ((decl->tag == STRUCT_DECL || decl->tag == UNION_DECL)
        && NamesEqual(decl->name.name, name)) {
      return (TypeDecl*)decl;
    }
  }
  return NULL;
}

// LookupFunc --
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
FuncDecl* LookupFunc(Env* env, Name name)
{
  for (size_t i = 0; i < env->declc; ++i) {
    Decl* decl = env->declv[i];
    if (decl->tag == FUNC_DECL && NamesEqual(decl->name.name, name)) {
      return (FuncDecl*)decl;
    }
  }
  return NULL;
}

// LookupProc --
//
//   Look up the declaration of the process with the given name in the given
//   environment.
//
// Inputs:
//   env - The environment to look up the type in.
//   name - The name of the process to look up.
//
// Result:
//   The declaration for the process with the given name, or NULL if there is
//   no process with the given name in the given environment.
//
// Side effects:
//   None.

ProcDecl* LookupProc(Env* env, Name name)
{
  for (size_t i = 0; i < env->declc; ++i) {
    Decl* decl = env->declv[i];
    if (decl->tag == PROC_DECL && NamesEqual(decl->name.name, name)) {
      return (ProcDecl*)decl;
    }
  }
  return NULL;
}
