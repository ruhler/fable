
#ifndef TRUTH_TABLE_H_
#define TRUTH_TABLE_H_

#include <cstdint>
#include <string>
#include <vector>

class TruthTable {
 public:
  // Constructs a truth table with the given inputs, outputs, and data
  // supplied in the given table.
  // The table should contain 2^inputs.size() elements. Each element gives the
  // bit pattern for the output in the least significant bits corresponding to
  // the bit pattern for the input.
  //
  // For example, consider the truth table for XOR:
  //   A B | Z = XOR(A,B)
  //   --------------
  //   0 0 | 0
  //   0 1 | 1
  //   1 0 | 1
  //   1 1 | 0
  // 
  // This is constructed with:
  //  inputs = "A", "B"
  //  outputs = "Z"
  //  table[0] = 0;
  //  table[1] = 1;
  //  table[2] = 1;
  //  table[3] = 0;
  //
  // The maximum number of inputs supported by this implementation is 32.
  // The maximum number of outputs supported by this implementation is 32.
  //
  // TODO: Should we require the input and output names to be unique?
  TruthTable(std::vector<std::string> inputs, std::vector<std::string> outputs,
      std::vector<uint32_t> table);
  
  // Return the value in the truth table for the given input bits.
  uint32_t Eval(uint32_t input) const;

  const int kNumInputs;
  const int kNumOutputs;

  const std::vector<std::string>& Inputs() const;
  const std::vector<std::string>& Outputs() const;

  // Truth tables are the same if they have the same input names, output
  // names, and table values.
  bool operator==(const TruthTable& rhs) const;
  bool operator!=(const TruthTable& rhs) const;

 private:
  const std::vector<std::string> inputs_;
  const std::vector<std::string> outputs_;
  std::vector<uint32_t> table_;
};

#endif//TRUTH_TABLE_H_

