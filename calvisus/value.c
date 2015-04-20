
value_t* mk_struct(int num_fields) {
  value_t* value = malloc(sizeof(int) + num_fields * sizeof(value_t*));
  value->field = FIELD_STRUCT;
  return value;
}

value_t* mk_union(int field) {
  value_t* value = malloc(sizeof(int) + sizeof(value_t*));
  value->field = field;
}

