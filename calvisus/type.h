
#ifndef TYPE_H_
#define TYPE_H_

#include "FblcInternal.h"

typedef enum { KIND_UNION, KIND_STRUCT } kind_t;

typedef struct {
  FblcName type;
  FblcName name;
} field_t;

typedef struct {
  FblcName name;
  kind_t kind;
  int num_fields;
  field_t fields[];
} type_t;

int indexof(const type_t* type, FblcName field);

#endif//TYPE_H_

