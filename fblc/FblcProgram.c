// FblcProgram.c --
//
//   The file contains utilities for working with the abstract syntax for Fblc
//   programs.

#include "FblcInternal.h"

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
    if (FblcNamesEqual(tenv->decl->name, name)) {
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
    if (FblcNamesEqual(fenv->decl->name, name)) {
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
//   None.
//
// Side effects:
//   The type declaration is added to the environment.

void FblcAddType(FblcEnv* env, FblcType* type)
{
  TypeEnv* types = GC_MALLOC(sizeof(TypeEnv));
  types->decl = type;
  types->next = env->types;
  env->types = types;
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
//   None.
//
// Side effects:
//   The function declaration is added to the environment.

void FblcAddFunc(FblcEnv* env, FblcFunc* func)
{
  FuncEnv* funcs = GC_MALLOC(sizeof(FuncEnv));
  funcs->decl = func;
  funcs->next = env->funcs;
  env->funcs = funcs;
}
