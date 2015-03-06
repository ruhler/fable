
#include "androcles/value.h"

#include "error.h"

class Value_ {
 public:
  Value_(Type type);
  virtual ~Value_();

  Type GetType() const;
  virtual Value GetField(const std::string& field_name) const = 0;
  virtual Value Select(const std::vector<Value>& choices) const = 0;
  virtual bool IsPartiallyUndefined() const = 0;
  virtual bool IsCompletelyUndefined() const = 0;
  virtual Value Copy() const = 0;

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
  virtual Value Select(const std::vector<Value>& choices) const;
  virtual bool IsPartiallyUndefined() const;
  virtual bool IsCompletelyUndefined() const;
  virtual Value Copy() const;
};

UndefinedValue::UndefinedValue(Type type)
  : Value_(type)
{}

UndefinedValue::~UndefinedValue()
{}

Value UndefinedValue::GetField(const std::string& field_name) const {
  Type field_type = GetType().TypeOfField(field_name);
  //CHECK_NE(Type::Null(), field_type);
  return Value::Undefined(field_type);
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

class StructValue : public Value_ {
 public:
  StructValue(Type type, const std::vector<Value>& fields);
  virtual ~StructValue();
  virtual Value GetField(const std::string& field_name) const;
  virtual Value Select(const std::vector<Value>& choices) const;
  virtual bool IsPartiallyUndefined() const;
  virtual bool IsCompletelyUndefined() const;
  virtual Value Copy() const;

 private:
  std::vector<Value> fields_;
};

StructValue::StructValue(Type type, const std::vector<Value>& fields)
 : Value_(type), fields_(fields) {
  CHECK_EQ(type.NumFields(), fields.size());
  for (int i = 0; i < type.NumFields(); i++) {
    //CHECK_EQ(type.TypeOfField(i), fields[i].GetType());
  }
}

StructValue::~StructValue()
{}

Value StructValue::GetField(const std::string& field_name) const {
  int index = GetType().IndexOfField(field_name);
  CHECK_NE(-1, index);
  return fields_[index];
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
  return true;
}

Value StructValue::Copy() const {
  return Value::Struct(GetType(), fields_);
}

class UnionValue : public Value_ {
 public:
  UnionValue(Type type, const std::string& field_name, const Value& value);
  virtual ~UnionValue();
  virtual Value GetField(const std::string& field_name) const;
  virtual Value Select(const std::vector<Value>& choices) const;
  virtual bool IsPartiallyUndefined() const;
  virtual bool IsCompletelyUndefined() const;
  virtual Value Copy() const;

 private:
  std::string field_name_;
  Value value_;
};

UnionValue::UnionValue(Type type, const std::string& field_name, const Value& value)
 : Value_(type), field_name_(field_name), value_(value) {
  Type field_type = type.TypeOfField(field_name);
  //CHECK_NE(Type::Null(), field_type);
  //CHECK_EQ(field_type, value.GetType());
}

UnionValue::~UnionValue()
{}

Value UnionValue::GetField(const std::string& field_name) const {
  Type field_type = GetType().TypeOfField(field_name);
  //CHECK_NE(Type::Null(), field_type);
  if (field_name == field_name_) {
    return value_;
  }
  return Value::Undefined(field_type);
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

Value::Value(const Value& rhs)
  : Value(std::move(rhs.value_->Copy()))
{}

Value::Value(std::unique_ptr<const Value_> value)
  : value_(std::move(value))
{}

Value& Value::operator=(const Value& rhs) {
  return *this = std::move(rhs.value_->Copy());
}

Type Value::GetType() const {
  return value_->GetType();
}

Value Value::GetField(const std::string& field_name) const {
  return value_->GetField(field_name);
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

Value Value::Undefined(Type type) {
  return Value(std::unique_ptr<const Value_>(new UndefinedValue(type)));
}

Value Value::Struct(Type type, const std::vector<Value>& fields) {
  return Value(std::unique_ptr<const Value_>(new StructValue(type, fields)));
}

Value Value::Union(Type type, const std::string& field_name, const Value& value) {
  return Value(std::unique_ptr<const Value_>(new UnionValue(type, field_name, value)));
}

