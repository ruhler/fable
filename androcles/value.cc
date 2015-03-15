
#include "androcles/value.h"

#include "error.h"

namespace androcles {

class StructValue;
class UnionValue;

class Value_ {
 public:
  Value_(Type type);
  virtual ~Value_();

  Type GetType() const;
  virtual Value GetField(const std::string& field_name) const = 0;
  virtual const std::string& GetTag() const = 0;
  virtual Value Select(const std::vector<Value>& choices) const = 0;
  virtual bool IsPartiallyUndefined() const = 0;
  virtual bool IsCompletelyUndefined() const = 0;
  virtual Value Copy() const = 0;

  virtual bool Equals(const Value_* rhs) const = 0;
  virtual bool EqualsStruct(const StructValue* rhs) const = 0;
  virtual bool EqualsUnion(const UnionValue* rhs) const = 0;

  virtual std::ostream& Print(std::ostream& os) const = 0;

 private:
  Type type_;
};

Value_::Value_(Type type)
  : type_(type)
{}

Value_::~Value_()
{}

Type Value_::GetType() const {
  return type_;
}

class UndefinedValue : public Value_ {
 public:
  UndefinedValue(Type type);
  virtual ~UndefinedValue();
  virtual Value GetField(const std::string& field_name) const;
  virtual const std::string& GetTag() const;
  virtual Value Select(const std::vector<Value>& choices) const;
  virtual bool IsPartiallyUndefined() const;
  virtual bool IsCompletelyUndefined() const;
  virtual Value Copy() const;
  virtual bool Equals(const Value_* rhs) const;
  virtual bool EqualsStruct(const StructValue* rhs) const;
  virtual bool EqualsUnion(const UnionValue* rhs) const;
  virtual std::ostream& Print(std::ostream& os) const;
};

UndefinedValue::UndefinedValue(Type type)
  : Value_(type)
{}

UndefinedValue::~UndefinedValue()
{}

Value UndefinedValue::GetField(const std::string& field_name) const {
  Type field_type = GetType().TypeOfField(field_name);
  CHECK_NE(Type::Null(), field_type);
  return Value::Undefined(field_type);
}

const std::string& UndefinedValue::GetTag() const {
  CHECK(false) << "GetTag called on undefined value";
}

Value UndefinedValue::Select(const std::vector<Value>& choices) const {
  Type type = GetType();
  CHECK_EQ(kUnion, type.GetKind());
  CHECK_EQ(type.NumFields(), choices.size());
  CHECK(!choices.empty());
  return Value::Undefined(choices.front().GetType());
}

bool UndefinedValue::IsPartiallyUndefined() const {
  return true;
}

bool UndefinedValue::IsCompletelyUndefined() const {
  return true;
}

Value UndefinedValue::Copy() const {
  return Value::Undefined(GetType());
}

bool UndefinedValue::Equals(const Value_* rhs) const {
  return false;
}

bool UndefinedValue::EqualsStruct(const StructValue* rhs) const {
  return false;
}

bool UndefinedValue::EqualsUnion(const UnionValue* rhs) const {
  return false;
}

std::ostream& UndefinedValue::Print(std::ostream& os) const {
  return os << "???";
}

class StructValue : public Value_ {
 public:
  StructValue(Type type, const std::vector<Value>& fields);
  virtual ~StructValue();
  virtual Value GetField(const std::string& field_name) const;
  virtual const std::string& GetTag() const;
  virtual Value Select(const std::vector<Value>& choices) const;
  virtual bool IsPartiallyUndefined() const;
  virtual bool IsCompletelyUndefined() const;
  virtual Value Copy() const;
  virtual bool Equals(const Value_* rhs) const;
  virtual bool EqualsStruct(const StructValue* rhs) const;
  virtual bool EqualsUnion(const UnionValue* rhs) const;
  virtual std::ostream& Print(std::ostream& os) const;

 private:
  std::vector<Value> fields_;
};

StructValue::StructValue(Type type, const std::vector<Value>& fields)
 : Value_(type), fields_(fields) {
  CHECK_EQ(type.NumFields(), fields.size());
  for (int i = 0; i < type.NumFields(); i++) {
    CHECK_EQ(type.TypeOfField(i), fields[i].GetType());
  }
}

StructValue::~StructValue()
{}

Value StructValue::GetField(const std::string& field_name) const {
  int index = GetType().IndexOfField(field_name);
  CHECK_NE(-1, index);
  return fields_[index];
}

const std::string& StructValue::GetTag() const {
  CHECK(false) << "GetTag called on struct value";
}

Value StructValue::Select(const std::vector<Value>& choices) const {
  CHECK(false) << "Select: Expected union value, but found struct value";
}

bool StructValue::IsPartiallyUndefined() const {
  for (int i = 0; i < fields_.size(); i++) {
    if (fields_[i].IsPartiallyUndefined()) {
      return true;
    }
  }
  return false;
}

bool StructValue::IsCompletelyUndefined() const {
  for (int i = 0; i < fields_.size(); i++) {
    if (!fields_[i].IsCompletelyUndefined()) {
      return false;
    }
  }
  return !fields_.empty();
}

Value StructValue::Copy() const {
  return Value::Struct(GetType(), fields_);
}

bool StructValue::Equals(const Value_* rhs) const {
  return rhs->EqualsStruct(this);
}

bool StructValue::EqualsStruct(const StructValue* rhs) const {
  if (GetType() != rhs->GetType()) {
    return false;
  }

  if (fields_.size() != rhs->fields_.size()) {
    return false;
  }

  for (int i = 0; i < fields_.size(); i++) {
    if (fields_[i] != rhs->fields_[i]) {
      return false;
    }
  }
  return true;
}

bool StructValue::EqualsUnion(const UnionValue* rhs) const {
  return false;
}

std::ostream& StructValue::Print(std::ostream& os) const {
  os << GetType().GetName() << "(";
  std::string comma = "";
  for (auto& field : fields_) {
    os << comma << field;
    comma = ", ";
  }
  return os << ")";
}

class UnionValue : public Value_ {
 public:
  UnionValue(Type type, const std::string& field_name, const Value& value);
  virtual ~UnionValue();
  virtual Value GetField(const std::string& field_name) const;
  virtual const std::string& GetTag() const;
  virtual Value Select(const std::vector<Value>& choices) const;
  virtual bool IsPartiallyUndefined() const;
  virtual bool IsCompletelyUndefined() const;
  virtual Value Copy() const;
  virtual bool Equals(const Value_* rhs) const;
  virtual bool EqualsStruct(const StructValue* rhs) const;
  virtual bool EqualsUnion(const UnionValue* rhs) const;
  virtual std::ostream& Print(std::ostream& os) const;

 private:
  std::string field_name_;
  Value value_;
};

UnionValue::UnionValue(Type type, const std::string& field_name, const Value& value)
 : Value_(type), field_name_(field_name), value_(value) {
  Type field_type = type.TypeOfField(field_name);
  CHECK_NE(Type::Null(), field_type);
  CHECK_EQ(field_type, value.GetType());
}

UnionValue::~UnionValue()
{}

Value UnionValue::GetField(const std::string& field_name) const {
  Type field_type = GetType().TypeOfField(field_name);
  CHECK_NE(Type::Null(), field_type);
  if (field_name == field_name_) {
    return value_;
  }
  return Value::Undefined(field_type);
}

const std::string& UnionValue::GetTag() const {
  return field_name_;
}

Value UnionValue::Select(const std::vector<Value>& choices) const {
  Type type = GetType();
  CHECK_EQ(type.NumFields(), choices.size());
  return choices[type.IndexOfField(field_name_)];
}

bool UnionValue::IsPartiallyUndefined() const {
  return value_.IsPartiallyUndefined();
}

bool UnionValue::IsCompletelyUndefined() const {
  return value_.IsCompletelyUndefined();
}

Value UnionValue::Copy() const {
  return Value::Union(GetType(), field_name_, value_);
}

bool UnionValue::Equals(const Value_* rhs) const {
  return rhs->EqualsUnion(this);
}

bool UnionValue::EqualsStruct(const StructValue* rhs) const {
  return false;
}

bool UnionValue::EqualsUnion(const UnionValue* rhs) const {
  if (GetType() != rhs->GetType()) {
    return false;
  }

  if (field_name_ != rhs->field_name_) {
    return false;
  }

  if (value_ != rhs->value_) {
    return false;
  }
  return true;
}

std::ostream& UnionValue::Print(std::ostream& os) const {
  os << GetType().GetName() << ":" << field_name_ << "(" << value_ << ")";
  return os;
}

Value::Value(const Value& rhs)
  : Value(std::move(rhs.value_->Copy()))
{}

Value::Value(std::unique_ptr<const Value_> value)
  : value_(std::move(value))
{
  CHECK(value_.get() != nullptr);
}

Value::Value(Value&&) = default;

Value::~Value()
{}

Value& Value::operator=(const Value& rhs) {
  return *this = std::move(rhs.value_->Copy());
}

Value& Value::operator=(Value&&) = default;

Type Value::GetType() const {
  return value_->GetType();
}

Value Value::GetField(const std::string& field_name) const {
  return value_->GetField(field_name);
}

const std::string& Value::GetTag() const {
  return value_->GetTag();
}

Value Value::Select(const std::vector<Value>& choices) const {
  return value_->Select(choices);
}

bool Value::IsPartiallyUndefined() const {
  return value_->IsPartiallyUndefined();
}

bool Value::IsCompletelyUndefined() const {
  return value_->IsCompletelyUndefined();
}

bool Value::operator==(const Value& rhs) const {
  return value_->Equals(rhs.value_.get());
}

bool Value::operator!=(const Value& rhs) const {
  return !(*this == rhs);
}

std::ostream& Value::Print(std::ostream& os) const {
  return value_->Print(os);
}

Value Value::Undefined(Type type) {
  return Value(std::unique_ptr<const Value_>(new UndefinedValue(type)));
}

Value Value::Struct(Type type, const std::vector<Value>& fields) {
  return Value(std::unique_ptr<const Value_>(new StructValue(type, fields)));
}

Value Value::Union(Type type, const std::string& field_name, const Value& value) {
  return Value(std::unique_ptr<const Value_>(new UnionValue(type, field_name, value)));
}

std::ostream& operator<<(std::ostream& os, const Value& rhs) {
  return rhs.Print(os);
}

}  // namespace androcles

