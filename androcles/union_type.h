
#ifndef UNION_TYPE_H_
#define UNION_TYPE_H_

#include <string>
#include <vector>

#include "androcles/field.h"
#include "androcles/type.h"

class Verification;

class UnionType : public Type {
 public:
  UnionType(const std::string& name, std::vector<Field>& fields);
  virtual ~UnionType();

  virtual void Verify(Verification& verification) const;
  virtual std::ostream& operator<<(std::ostream& os) const;

  const std::string& GetName() const;
  const std::vector<field>& GetFields() const;

 private:
  const std::string name_;
  const std::vector<Field> fields_;
};

#endif//UNION_TYPE_H_

