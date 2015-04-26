
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

FblcEnv* FblcNewEnv() {
  return GC_MALLOC(sizeof(FblcEnv));
}

FblcType* FblcLookupType(const FblcEnv* env, FblcName name) {
  for (TypeEnv* tenv = env->types; tenv != NULL; tenv = tenv->next) {
    if (FblcNamesEqual(tenv->decl->name, name)) {
      return tenv->decl;
    }
  }
  return NULL;
}

FblcFunc* FblcLookupFunc(const FblcEnv* env, FblcName name) {
  for (FuncEnv* fenv = env->funcs; fenv != NULL; fenv = fenv->next) {
    if (FblcNamesEqual(fenv->decl->name, name)) {
      return fenv->decl;
    }
  }
  return NULL;
}

void FblcAddType(FblcEnv* env, FblcType* type) {
  TypeEnv* types = GC_MALLOC(sizeof(TypeEnv));
  types->decl = type;
  types->next = env->types;
  env->types = types;
}

void FblcAddFunc(FblcEnv* env, FblcFunc* func) {
  FuncEnv* funcs = GC_MALLOC(sizeof(FuncEnv));
  funcs->decl = func;
  funcs->next = env->funcs;
  env->funcs = funcs;
}

