
#include "circuit/adder.h"

#include <sstream>

#include "error.h"
#include "circuit/circuit.h"
#include "circuit/truth_table_component.h"
#include "truth_table/truth_table.h"

// Create a full 1-bit adder.
std::unique_ptr<Component> CreateFullAdder()
{
  std::vector<std::string> inputs{"A", "B", "Cin"};
  std::vector<std::string> outputs{"Z", "Cout"};
  std::vector<uint32_t> table(8);
  table[0] = 0;  // 0+0+0 = 0 carry 0
  table[1] = 2;  // 0+0+1 = 1 carry 0
  table[2] = 2;  // 0+1+0 = 1 carry 0
  table[3] = 1;  // 0+1+1 = 0 carry 1
  table[4] = 2;  // 1+0+0 = 1 carry 0
  table[5] = 1;  // 1+0+1 = 0 carry 1
  table[6] = 1;  // 1+1+0 = 0 carry 1
  table[7] = 3;  // 1+1+1 = 1 carry 1
  TruthTable truth_table(inputs, outputs, table);
  return std::unique_ptr<Component>(new TruthTableComponent(truth_table));
}

std::unique_ptr<Component> CreateAdder(int n)
{
  CHECK_GT(n, 0) << "CreateAdder n must be greater than zero.";

  std::unique_ptr<Component> adder1 = CreateFullAdder();

  // Chain n adder1s together by their carry outs and ins.
  // Entry 0 adds the least significant bits.
  // Entry (n-1) adds the most significant bits.
  std::vector<Circuit::SubComponentEntry> components;
  for (int i = 0; i < n; i++) {
    Circuit::SubComponentEntry entry;
    entry.component = adder1.get();

    Circuit::PortIdentifier ain;
    ain.component_index = Circuit::kInputPortComponentIndex;
    ain.port_index = (n-i-1);
    entry.inputs.push_back(ain);

    Circuit::PortIdentifier bin;
    bin.component_index = Circuit::kInputPortComponentIndex;
    bin.port_index = n+(n-i-1);
    entry.inputs.push_back(bin);

    Circuit::PortIdentifier cin;
    if (i == 0) {
      cin.component_index = Circuit::kInputPortComponentIndex;
      cin.port_index = 2*n;
    } else {
      cin.component_index = i-1;
      cin.port_index = 1;
    }
    entry.inputs.push_back(cin);

    components.push_back(entry);
  }

  // Output bits come one each from each of the adders.
  // They come in reverse order of the components, because it goes most
  // significant bit to least significant bit.
  std::vector<Circuit::PortIdentifier> outvals;
  for (int i = 1; i <= n; i++) {
    Circuit::PortIdentifier portid;
    portid.component_index = n-i;
    portid.port_index = 0;
    outvals.push_back(portid);
  }

  // The carry out comes from the carry out of the last adder.
  Circuit::PortIdentifier portid;
  portid.component_index = n-1;
  portid.port_index = 1;
  outvals.push_back(portid);

  // Transfer ownership of the full adder to the circuit.
  std::vector<std::unique_ptr<Component>> owned;
  owned.push_back(std::move(adder1));

  std::vector<std::string> inputs;
  for (int i = n-1; i >= 0; i--) {
    std::ostringstream oss;
    oss << "A" << i;
    inputs.push_back(oss.str());
  }
  for (int i = n-1; i >= 0; i--) {
    std::ostringstream oss;
    oss << "B" << i;
    inputs.push_back(oss.str());
  }
  inputs.push_back("Cin");
  
  std::vector<std::string> outputs;
  for (int i = n-1; i >= 0; i--) {
    std::ostringstream oss;
    oss << "Z" << i;
    outputs.push_back(oss.str());
  }
  outputs.push_back("Cout");

  return std::unique_ptr<Component>(
      new Circuit(inputs, outputs, components, outvals, std::move(owned)));
}

