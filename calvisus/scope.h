
#ifndef SCOPE_H_
#define SCOPE_H_

#include "name.h"
#include "value.h"

typedef struct scope_t {
  vname_t name;
  value_t* value;
  struct scope_t* next;
} scope_t;

value_t* lookup_var(scope_t* scope, vname_t name);
scope_t* extend(scope_t* scope, vname_t name, value_t* value);

void dump_scope(FILE* fout, scope_t* scope);

#endif//SCOPE_H_

