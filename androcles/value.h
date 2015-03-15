
#ifndef ANDROCLES_VALUE_H_
#define ANDROCLES_VALUE_H_

#include <memory>
#include <string>
#include <vector>

#include "androcles/type.h"

namespace androcles {

class Value_;

class Value {
 public:
  Value(const Value& rhs);
  Value(Value&&);
  ~Value();

  Value& operator=(const Value& rhs);
  Value& operator=(Value&&);

  // Returns the type of the value.
  Type GetType() const;

  // Returns the value of the given field.
  // Returns an undefined value if this value is undefined or is a union value
  // whose tag does not match the field.
  // It is an error to call GetField with a field_name that does not
  // belong to the type of the value.
  Value GetField(const std::string& field_name) const;

  // Returns the tag for a union value.
  // It is an error to call GetTag if the value is undefined or is not of
  // union type.
  const std::string& GetTag() const;

  // Select the value from the choices based on the currently active field of
  // this valie.
  // Returns an undefined value if this value is undefined or is a struct
  // value.
  // It is an error to call Select with a different number of choices than
  // there are fields in this value's type. There must be at least one choice
  // given.
  Value Select(const std::vector<Value>& choices) const;

  // Return true if any part of the value is undefined.
  bool IsPartiallyUndefined() const;

  // Return true if the value is completely undefined.
  bool IsCompletelyUndefined() const;

  // Compares two values for equality.
  // Note: Undefined values are not considered equal to any other value,
  // including other undefined values.
  bool operator==(const Value& rhs) const;
  bool operator!=(const Value& rhs) const;

  // Returns a completely undefined value of the given type.
  static Value Undefined(Type type);

  // Returns a struct value with given field values.
  // It is an error if the type is not a struct type, or if the number and
  // types of fields does not match the number and types of fields in the
  // struct type.
  static Value Struct(Type type, const std::vector<Value>& fields);

  // Returns a union value with given field value.
  // It is an error if the type is not a union type, or if the field name or
  // type does not match a field name and type in the union type.
  static Value Union(Type type, const std::string& field_name, const Value& value);

 private:
  Value(std::unique_ptr<const Value_> value);

  std::unique_ptr<const Value_> value_;
};

}  // namespace androcles

#endif//ANDROCLES_VALUE_H_

