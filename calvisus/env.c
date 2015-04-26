
#include "env.h"

#include "FblcInternal.h"

FblcType* lookup_type(const env_t* env, FblcName name) {
  for (type_env_t* tenv = env->types; tenv != NULL; tenv = tenv->next) {
    if (FblcNamesEqual(tenv->decl->name, name)) {
      return tenv->decl;
    }
  }
  return NULL;
}

func_t* lookup_func(const env_t* env, FblcName name) {
  for (func_env_t* fenv = env->funcs; fenv != NULL; fenv = fenv->next) {
    if (FblcNamesEqual(fenv->decl->name, name)) {
      return fenv->decl;
    }
  }
  return NULL;
}

