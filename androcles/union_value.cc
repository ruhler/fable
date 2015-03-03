
#include "union_value.h"

#include <string>

#include "androcles/value.h"
#include "androcles/union_type.h"

UnionValue::UnionValue(const UnionType* type, const std::string& field, std::unique_ptr<const Value> value)
  : type_(type), field_(field), value_(std::move(value))
{}

const UnionType* UnionValue::GetType() const {
  return type_;
}

