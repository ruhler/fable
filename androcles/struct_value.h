
#ifndef STRUCT_VALUE_H_
#define STRUCT_VALUE_H_

#include <vector>

#include "androcles/struct_type.h"
#include "androcles/value.h"


class StructValue : public Value {
 public:
  StructValue(const StructType* type, const std::vector<const Value*> args);
  virtual ~StructValue();
  virtual const StructType* GetType() const;

 private:
  const StructType* type_;
  const std::vector<const Value*> args_;
};

#endif//STRUCT_VALUE_H_

