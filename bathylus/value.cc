
#include "bathylus/value.h"

#include "error.h"

Value::Value(const Type* type, int tag, const std::vector<Value>& fields)
  : type_(type), tag_(tag), fields_(fields)
{}

Value Value::Struct(const Type* type, const std::vector<Value>& fields) {
  return Value(type, TAG_STRUCT, fields);
}

Value Value::Union(const Type* type, int tag, const Value& content) {
  return Value(type, tag, {content});
}

Value Value::Undefined(const Type* type) {
  return Value(type, TAG_UNDEFINED, {});
}

int Value::GetTag() const {
  CHECK_NE(tag_, TAG_STRUCT) << "GetTag called on Struct Value";
  CHECK_NE(tag_, TAG_UNDEFINED) << "GetTag called on Undefined Value";
  return tag_;
}

bool Value::operator==(const Value& rhs) const {
  return type_ == rhs.type_ && tag_ == rhs.tag_ && fields_ == rhs.fields_;
}

bool Value::operator!=(const Value& rhs) const {
  return !this->operator==(rhs);
}

