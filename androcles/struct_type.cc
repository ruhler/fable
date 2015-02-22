
#include "androcles/struct_type.h"

#include <string>
#include <vector>

#include "androcles/field.h"
#include "androcles/verification.h"

StructType::StructType(const std::string& name, const std::vector<Field>& fields)
  : name_(name), fields_(fields)
{}

StructType::~StructType()
{}

void StructType::Verify(Verification& verification) const {
  VerifyFieldNamesAreDistinct(fields_, verification);
}

std::ostream& StructType::operator<<(std::ostream& os) const {
  return os << name_;
}

const std::string& StructType::GetName() const {
  return name_;
}

const std::vector<Field>& StructType::GetFields() const {
  return fields_;
}

