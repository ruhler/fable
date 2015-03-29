
#ifndef BATHYLUS_TYPE_H_
#define BATHYLUS_TYPE_H_

#include <string>
#include <vector>

enum Kind {
  kStruct,
  kUnion
};

class Type;

struct Field {
  const Type* type;
  std::string name;
};

class Type {
 public:
  Type(Kind kind, const std::string& name, const std::vector<Field>& fields);

  const std::string& GetName() const;
  const Type* TypeOfField(int tag) const;
  const std::string& NameOfField(int tag) const;

 private:
  Kind kind_;
  std::string name_;
  std::vector<Field> fields_;
};

#endif//BATHYLUS_TYPE_H_

