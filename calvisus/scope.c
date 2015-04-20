
#include "scope.h"

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
  scope_t* newscope = malloc(sizeof(scope_t));
  newscope.name = name;
  newscope.value = value;
  newscope.next = scope;
  return newscope;
}

