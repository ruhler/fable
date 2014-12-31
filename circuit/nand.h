
#ifndef NAND_H_
#define NAND_H_

#include <vector>

#include "circuit.h"
#include "value.h"

class Nand : public Circuit {
 public:
  virtual std::vector<Value> Eval(const std::vector<Value>& inputs) const;
};

#endif//NAND_H_
