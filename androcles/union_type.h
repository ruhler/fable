
#ifndef UNION_TYPE_H_
#define UNION_TYPE_H_

#include <string>
#include <vector>

#include "androcles/field.h"
#include "androcles/type.h"

class UnionType : public Type {
 public:
  UnionType(const std::string& name, const std::vector<Field>& fields);
  virtual ~UnionType();
};

#endif//UNION_TYPE_H_

