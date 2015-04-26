
#include "type.h"

#include "FblcInternal.h"

int indexof(const type_t* type, FblcName field) {
  for (int i = 0; i < type->num_fields; i++) {
    if (FblcNamesEqual(field, type->fields[i].name)) {
      return i;
    }
  }
  assert(false && "No such field.");
}

