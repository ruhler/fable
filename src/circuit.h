
#ifndef CIRCUIT_H_
#define CIRCUIT_H_

#include <vector>

#include "value.h"

class Component {
 public:
  Component(int num_inputs, int num_outputs);
  virtual std::vector<Value> Eval(const std::vector<Value>& inputs) const = 0;

  const int kNumInputs;
  const int kNumOutputs;
};

// A Circuit is an interconnection of Components.
class Circuit : public Component {
 public:
  // A port identifier identifies a source of data.
  //
  // The component_index is an index into the sub_components array
  // identifying which sub component that data comes from. The index
  // kInputPortComponentIndex is used to specify data that comes from an input
  // to the Circuit.
  //
  // The component_index must be smaller than the index of the current sub
  // component entry to ensure there are no loops in the circuit.
  //
  // The port_index specifies the port index for the specified component.
  struct PortIdentifier {
    int component_index;
    int port_index;
  };

  // A SubComponentEntry species an instance of a component in the circuit.
  // The component indicates what kind of component to instantiate.
  // The inputs describe the source of input data for the instantiated
  // component. Each input port identifier describes the source of data for
  // the corresponding input port of the component. The number of elements in
  // the inputs vector must match the number of inputs in the component.
  struct SubComponentEntry {
    const Component* component;
    std::vector<PortIdentifier> inputs;
  };

  // Component index that should be used to refer to an input port of the
  // circuit.
  static const int kInputPortComponentIndex = -1;

  // A Circuit is specified with a list of sub component entries and output
  // port identifiers. The outputs vector describes the source of data for the
  // output of the circuit.
  //
  // The user must ensure the sub components used stay valid for the lifetime
  // of the constructed circuit object.
  Circuit(int num_inputs,
      std::vector<SubComponentEntry> sub_components,
      std::vector<PortIdentifier> outputs);

  virtual std::vector<Value> Eval(const std::vector<Value>& inputs) const;

 private:
  std::vector<SubComponentEntry> sub_components_;
  std::vector<PortIdentifier> outputs_;
};

#endif//CIRCUIT_H_
