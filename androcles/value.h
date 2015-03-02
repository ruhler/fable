
#ifndef VALUE_H_
#define VALUE_H_

class Type;

class Value {
 public:
  virtual ~Value();

  // Returns the type of the value.
  virtual const Type* GetType() const = 0;
};

#endif//VALUE_H_

