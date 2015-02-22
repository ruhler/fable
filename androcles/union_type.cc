
#include "androcles/struct_type.h"

#include <string>
#include <vector>

#include "androcles/field.h"
#include "error.h"

UnionType::UnionType(const std::string& name, const std::vector<Field>& fields)
  : name_(name), fields_(fields)
{}

UnionType::~UnionType()
{}

void UnionType::Verify(Verification& verification) const {
  VerifyFieldNamesAreDistinct(fields_, verification);
}

std::ostream& UnionType::operator<<(std::ostream& os) const {
  return os << name_;
}

const std::string& UnionType::GetName() const {
  return name_;
}

const std::vector<Field>& UnionType::GetFields() const {
  return fields_;
}

