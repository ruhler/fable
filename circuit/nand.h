
#ifndef NAND_H_
#define NAND_H_

#include <vector>

#include "circuit.h"
#include "value.h"

class Nand : public Circuit {
  public:
    virtual std::vector<Value> run(const std::vector<Value>& inputs);
};

#endif//NAND_H_
