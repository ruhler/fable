
#include "androcles/type.h"

#include "error.h"

Type::Type(const std::string& name, const std::vector<Field>& fields)
  : name_(name), fields_(fields)
{}

Type::~Type()
{}

bool Type::operator==(const Type& rhs) const {
  // Use pointer equality for types.
  // We shouldn't ever be creating copies of the same type, so this will do
  // what we want.
  return (this == &rhs);
}

std::ostream& Type::operator<<(std::ostream& os) const {
  return os << name_;
}

const std::vector<Field>& Type::GetFields() const {
  return fields_;
}

const Type* Type::TypeOfField(const std::string& field) const {
  for (Field f : fields_) {
    if (f.name == field) {
      return f.type;
    }
  }
  CHECK(false) << "TypeOfField: bad field " << field << " for type " << name_;
}

