
#ifndef UNION_VALUE_H_
#define UNION_VALUE_H_

#include <memory>
#include <string>

#include "androcles/union_type.h"
#include "androcles/value.h"


class UnionValue : public Value {
 public:
  UnionValue(const UnionType* type, const std::string& field, std::unique_ptr<const Value> value);
  virtual ~UnionValue();
  virtual const UnionType* GetType() const;

 private:
  const UnionType* type_;
  const std::string field_;
  std::unique_ptr<const Value> value_;
};

#endif//UNION_VALUE_H_

