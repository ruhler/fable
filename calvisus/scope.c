
#include "scope.h"

#include <stdlib.h>

#include <gc/gc.h>

value_t* lookup_var(scope_t* scope, vname_t name) {
  if (scope == NULL) {
    return NULL;
  } else if (name_eq(scope->name, name)) {
    return scope->value;
  } else {
    return lookup_var(scope->next, name);
  }
}

scope_t* extend(scope_t* scope, vname_t name, value_t* value) {
  scope_t* newscope = GC_MALLOC(sizeof(scope_t));
  newscope->name = name;
  newscope->value = value;
  newscope->next = scope;
  return newscope;
}

void dump_scope(FILE* fout, scope_t* scope) {
  for ( ; scope != NULL; scope = scope->next) {
    fprintf(fout, " %s = ...\n", scope->name);
  }
}

