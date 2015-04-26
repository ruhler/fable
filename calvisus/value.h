
#ifndef VALUE_H_
#define VALUE_H_

#include "FblcInternal.h"

#define FIELD_STRUCT -1

// For struct value, field is FIELD_STRUCT and fields contains the field data.
// For union value, field is the index of the active field and fields is a
// single element array with the field value.
// In both cases the type of value is implied by the context.
typedef struct value_t {
  FblcType* type;
  int field;
  struct value_t* fields[];
} value_t;

value_t* mk_value(FblcType* type);
value_t* mk_union(FblcType* type, int field);

void print(FILE* fout, value_t* value);

#endif//VALUE_H_
