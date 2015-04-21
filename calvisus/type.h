
#ifndef TYPE_H_
#define TYPE_H_

#include "name.h"

typedef enum { KIND_UNION, KIND_STRUCT } kind_t;

typedef struct {
  dname_t type;
  fname_t name;
} field_t;

typedef struct {
  dname_t name;
  kind_t kind;
  int num_fields;
  field_t fields[];
} type_t;

int indexof(const type_t* type, fname_t field);

#endif//TYPE_H_

