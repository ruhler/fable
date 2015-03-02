
#include "androcles/struct_value.h"

StructValue::StructValue(const StructType* type, const std::vector<const Value*> args)
  : type_(type), args_(args)
{}

StructValue::~StructValue()
{}

const StructType* StructValue::GetType() const {
  return type_;
}

