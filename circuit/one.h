
#ifndef ONE_H_
#define ONE_H_

#include <vector>

#include "circuit.h"
#include "value.h"

class One : public Circuit {
  public:
    virtual std::vector<Value> run(const std::vector<Value>& inputs);
};

#endif//ONE_H_
