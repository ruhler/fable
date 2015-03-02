
#ifndef TYPE_H_
#define TYPE_H_

#include <string>
#include <iostream>
#include <vector>

#include "androcles/field.h"

// Each instance of 'Type' refers to a unique androcles type.
class Type {
 public:
  Type(const std::string& name, const std::vector<Field>& fields);
  virtual ~Type();

  bool operator==(const Type& rhs) const;
  std::ostream& operator<<(std::ostream& os) const;

  const std::string& GetName() const;
  const std::vector<Field>& GetFields() const;

  // Returns the type of the given field. 
  // It is an error if the field is not a valid field of the type.
  const Type* TypeOfField(const std::string& field) const;

 private:
  const std::string name_;
  const std::vector<Field> fields_;
};

#endif//TYPE_H_

