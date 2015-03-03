
#ifndef STRUCT_VALUE_H_
#define STRUCT_VALUE_H_

#include <memory>
#include <vector>

#include "androcles/struct_type.h"
#include "androcles/value.h"


class StructValue : public Value {
 public:
  StructValue(const StructType* type, std::vector<std::unique_ptr<const Value>> args);
  virtual ~StructValue();
  virtual const StructType* GetType() const;
  virtual std::unique_ptr<const Value> Copy() const;

 private:
  const StructType* type_;
  std::vector<std::unique_ptr<const Value>> args_;
};

#endif//STRUCT_VALUE_H_

