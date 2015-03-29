
#ifndef BATHYLUS_VALUE_H_
#define BATHYLUS_VALUE_H_

#include <iostream>
#include <vector>

#include "bathylus/type.h"

class Value {
 public:
  static Value Struct(const Type* type, const std::vector<Value>& fields);
  static Value Union(const Type* type, int tag, const Value& content);
  static Value Undefined(const Type* type);

  int GetTag() const;
  Value Access(int tag) const;

  bool operator==(const Value& rhs) const;
  bool operator!=(const Value& rhs) const;

 private:
  Value(const Type* type, int tag, const std::vector<Value>& fields);

  static const int TAG_UNDEFINED = -2;
  static const int TAG_STRUCT = -1;

  // Undefined: tag_ is TAG_UNDEFINED. fields_ is empty.
  // Struct: tag_ is TAG_STRUCT. fields_ contain the struct fields.
  // Union: tag_ is the union tag. fields_ has a single element which is the
  // content.
  const Type* type_;
  int tag_;
  std::vector<Value> fields_;

  friend std::ostream& operator<<(std::ostream& os, const Value& rhs);
};

#endif//BATHYLUS_VALUE_H_

