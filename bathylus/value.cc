
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

Value Value::Access(int tag) const {
  if (tag_ == TAG_STRUCT) {
    CHECK_LE(0, tag);
    CHECK_LT(tag, fields_.size());
    return fields_[tag];
  }

  if (tag_ == tag) {
    CHECK_EQ(1, fields_.size());
    return fields_[0];
  }

  // This value is undefined or it is a union with a different tag.
  // Either way, the result is an undefined value.
  return Value::Undefined(type_->TypeOfField(tag));
}

bool Value::operator==(const Value& rhs) const {
  return type_ == rhs.type_ && tag_ == rhs.tag_ && fields_ == rhs.fields_;
}

bool Value::operator!=(const Value& rhs) const {
  return !this->operator==(rhs);
}

