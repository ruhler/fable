
#include "androcles/struct_value.h"

#include <memory>

StructValue::StructValue(const StructType* type, std::vector<std::unique_ptr<const Value>> args)
  : type_(type), args_(std::move(args))
{}

StructValue::~StructValue()
{}

const StructType* StructValue::GetType() const {
  return type_;
}

std::unique_ptr<const Value> StructValue::Copy() const {
  std::vector<std::unique_ptr<const Value>> args_copy;
  for (auto& v : args_) {
    args_copy.push_back(v->Copy());
  }
  return std::unique_ptr<const Value>(new StructValue(type_, std::move(args_copy)));
}

