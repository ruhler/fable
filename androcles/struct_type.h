
#ifndef STRUCT_TYPE_H_
#define STRUCT_TYPE_H_

#include <string>
#include <vector>

#include "androcles/field.h"
#include "androcles/type.h"

class StructType : public Type {
 public:
  StructType(const std::string& name, const std::vector<Field>& fields);
  virtual ~StructType();
};

#endif//STRUCT_TYPE_H_

