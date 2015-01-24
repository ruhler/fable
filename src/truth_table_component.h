
#ifndef TRUTH_TABLE_COMPONENT_
#define TRUTH_TABLE_COMPONENT_

#include <cstdint>
#include <string>
#include <vector>

#include "circuit.h"
#include "truth_table.h"
#include "value.h"

class TruthTableComponent : public Component {
 public:
  TruthTableComponent(TruthTable truth_table);
  TruthTableComponent(std::vector<std::string> inputs,
      std::vector<std::string> outputs,
      std::vector<uint32_t> table);

  virtual std::vector<Value> Eval(const std::vector<Value>& inputs) const;
  virtual int NumInputs() const;
  virtual int NumOutputs() const;

 private:
  TruthTable truth_table_;
};

#endif//TRUTH_TABLE_COMPONENT_
