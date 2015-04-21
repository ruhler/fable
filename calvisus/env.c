
#include "env.h"

#include <stddef.h>

type_t* lookup_type(const env_t* env, dname_t name) {
  type_env_t* tenv = env->types;
  for (type_env_t* tenv = env->types; tenv != NULL; tenv = tenv->next) {
    if (name_eq(tenv->decl->name, name)) {
      return tenv->decl;
    }
  }
  return NULL;
}

func_t* lookup_func(const env_t* env, dname_t name) {
  func_env_t* fenv = env->funcs;
  for (func_env_t* fenv = env->funcs; fenv != NULL; fenv = fenv->next) {
    if (name_eq(fenv->decl->name, name)) {
      return fenv->decl;
    }
  }
  return NULL;
}

