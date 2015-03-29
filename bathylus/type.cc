
#include "bathylus/type.h"

#include "error.h"

Type::Type(Kind kind, const std::string& name, const std::vector<Field>& fields)
  : kind_(kind), name_(name), fields_(fields)
{}

const Type* Type::TypeOfField(int tag) const {
  CHECK_LE(0, tag);
  CHECK_LT(tag, fields_.size());
  return fields_[tag].type;
}

