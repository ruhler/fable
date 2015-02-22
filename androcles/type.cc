
#include "androcles/type.h"

Type::~Type()
{}

bool Type::operator==(const Type& rhs) const {
  // Use pointer equality for types.
  // We shouldn't ever be creating copies of the same type, so this will do
  // what we want.
  return (this == &rhs);
}

