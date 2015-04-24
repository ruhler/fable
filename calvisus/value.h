
#ifndef VALUE_H_
#define VALUE_H_

#include <stdio.h>

#include "type.h"

#define FIELD_STRUCT -1

// For struct value, field is FIELD_STRUCT and fields contains the field data.
// For union value, field is the index of the active field and fields is a
// single element array with the field value.
// In both cases the type of value is implied by the context.
typedef struct value_t {
  type_t* type;
  int field;
  struct value_t* fields[0];
} value_t;

value_t* mk_value(type_t* type);
value_t* mk_union(type_t* type, int field);

void print(FILE* fout, value_t* value);

#endif//VALUE_H_
