
#ifndef CIRCUIT_H_
#define CIRCUIT_H_

#include <vector>

#include "value.h"

class Circuit {
 public:
  virtual std::vector<Value> Eval(const std::vector<Value>& inputs) const = 0;
};

#endif//CIRCUIT_H_
