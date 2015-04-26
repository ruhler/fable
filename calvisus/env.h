
#ifndef ENV_H_
#define ENV_H_

#include "expr.h"
#include "type.h"

#include "FblcInternal.h"

typedef struct {
  FblcName name;      // Name of function.
  FblcName rtype;     // Name of return type.
  expr_t* body;
  int num_args;
  field_t args[];
} func_t;

typedef struct type_env_t {
  type_t* decl;
  struct type_env_t* next;
} type_env_t;

typedef struct func_env_t {
  func_t* decl;
  struct func_env_t* next;
} func_env_t;

typedef struct {
  type_env_t* types;
  func_env_t* funcs;
} env_t;

type_t* lookup_type(const env_t* env, FblcName name);
func_t* lookup_func(const env_t* env, FblcName name);

#endif//ENV_H_

