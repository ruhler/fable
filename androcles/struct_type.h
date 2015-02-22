
#ifndef STRUCT_TYPE_H_
#define STRUCT_TYPE_H_

#include <string>
#include <vector>

#include "androcles/field.h"
#include "androcles/type.h"

class Verification;

class StructType : public Type {
 public:
  StructType(const std::string& name, const std::vector<Field>& fields);
  virtual ~StructType();

  virtual void Verify(Verification& verification) const;
  virtual std::ostream& operator<<(std::ostream& os) const;

  const std::string& GetName() const;
  const std::vector<field>& GetFields() const;

 private:
  const std::string name_;
  const std::vector<Field> fields_;
};

#endif//STRUCT_TYPE_H_

