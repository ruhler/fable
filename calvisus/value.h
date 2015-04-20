
#ifndef VALUE_H_
#define VALUE_H_

#define FIELD_STRUCT -1

// For struct value, field is FIELD_STRUCT and fields contains the field data.
// For union value, field is the index of the active field and fields is a
// single element array with the field value.
// In both cases the type of value is implied by the context.
typedef struct value_t {
  int field;
  struct value_t* fields[];
} value_t;

value_t* mk_struct(int num_fields);
value_t* mk_union(int field);

#endif//VALUE_H_
