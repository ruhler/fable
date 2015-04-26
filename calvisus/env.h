
#ifndef ENV_H_
#define ENV_H_

#include "FblcInternal.h"

typedef struct type_env_t {
  FblcType* decl;
  struct type_env_t* next;
} type_env_t;

typedef struct func_env_t {
  FblcFunc* decl;
  struct func_env_t* next;
} func_env_t;

typedef struct {
  type_env_t* types;
  func_env_t* funcs;
} env_t;

FblcType* lookup_type(const env_t* env, FblcName name);
FblcFunc* lookup_func(const env_t* env, FblcName name);

#endif//ENV_H_

