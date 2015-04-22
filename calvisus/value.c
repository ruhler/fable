
#include "value.h"

#include <assert.h>
#include <stdlib.h>

value_t* mk_value(type_t* type) {
  int fields = type->num_fields;
  if (type->kind == KIND_UNION) {
    fields = 1;
  }
  value_t* value = malloc(sizeof(value_t) + type->num_fields * sizeof(value_t*));
  value->field = FIELD_STRUCT;
  return value;
}

value_t* mk_union(type_t* type, int field) {
  value_t* value = mk_value(type);
  value->field = field;
  return value;
}

void print(FILE* fout, value_t* value) {
  type_t* type = value->type;
  if (type->kind == KIND_STRUCT) {
    fprintf(fout, "%s(", type->name);
    bool comma = false;
    for (int i = 0; i < type->num_fields; i++) {
      if (i > 0) {
        fprintf(fout, ",");
      }
      print(fout, value->fields[i]);
    }
    fprintf(fout, ")");
    return;
  }
  
  if (type->kind == KIND_UNION) {
    fprintf(fout, "%s:%s(", type->name, type->fields[value->field].name);
    print(fout, value->fields[0]);
    fprintf(fout, ")");
    return;
  }

  assert(false && "Invalid Kind");
}

