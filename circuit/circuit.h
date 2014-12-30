
#ifndef CIRCUIT_H_
#define CIRCUIT_H_

#include <vector>

#include "value.h"

class Circuit {
  public:
    virtual std::vector<Value> run(const std::vector<Value>& inputs) = 0;
};

#endif//CIRCUIT_H_
