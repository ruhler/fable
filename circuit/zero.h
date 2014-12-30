
#ifndef ZERO_H_
#define ZERO_H_

#include <vector>

#include "circuit.h"
#include "value.h"

class Zero : public Circuit {
  public:
    virtual std::vector<Value> run(const std::vector<Value>& inputs) const;
};

#endif//ZERO_H_
