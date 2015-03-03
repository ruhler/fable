
#ifndef VALUE_H_
#define VALUE_H_

#include <memory>

class Type;

class Value {
 public:
  virtual ~Value();

  // Returns the type of the value.
  virtual const Type* GetType() const = 0;

  // Returns a deep copy of this value.
  virtual std::unique_ptr<const Value> Copy() const = 0;
};

#endif//VALUE_H_

