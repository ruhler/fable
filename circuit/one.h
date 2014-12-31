
#ifndef ONE_H_
#define ONE_H_

#include <vector>

#include "circuit.h"
#include "value.h"

class One : public Circuit {
 public:
  One();
  virtual std::vector<Value> Eval(const std::vector<Value>& inputs) const;
};

#endif//ONE_H_
