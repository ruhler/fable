
#ifndef SCOPE_H_
#define SCOPE_H_

#include "FblcInternal.h"
#include "value.h"

typedef struct scope_t {
  FblcName name;
  value_t* value;
  struct scope_t* next;
} scope_t;

value_t* lookup_var(scope_t* scope, FblcName name);
scope_t* extend(scope_t* scope, FblcName name, value_t* value);

void dump_scope(FILE* fout, scope_t* scope);

#endif//SCOPE_H_

