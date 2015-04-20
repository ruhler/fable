
#ifndef ENV_H_
#define ENV_H_

#include "name.h"
#include "type.h"

typedef struct {
  dname_t name;
  dname_t rtype;
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
  type_env_t types;
  func_env_t funcs;
} env_t;

type_t* lookup_type(env_t* env, dname_t name);
func_t* lookup_func(env_t* env, dname_t name);

#endif//ENV_H_

