
#include "type.h"

#include <assert.h>

int indexof(const type_t* type, fname_t field) {
  for (int i = 0; i < type->num_fields; i++) {
    if (name_eq(field, type->fields[i].name)) {
      return i;
    }
  }
  assert(false && "No such field.");
}

