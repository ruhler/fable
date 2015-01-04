
#ifndef CIRCUIT_H_
#define CIRCUIT_H_

#include <memory>
#include <vector>

#include "value.h"

class Component {
 public:
  virtual ~Component();
  virtual std::vector<Value> Eval(const std::vector<Value>& inputs) const = 0;
  virtual int NumInputs() const = 0;
  virtual int NumOutputs() const = 0;
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
  // of the constructed circuit object. Optionally, the user can transfer
  // ownership of some or all of the sub components to this circuit in the
  // 'owned_components' vector. These will be deleted by the circuit when it
  // is deleted.
  Circuit(int num_inputs,
      std::vector<SubComponentEntry> sub_components,
      std::vector<PortIdentifier> outputs,
      std::vector<std::unique_ptr<Component>> owned_components);

  // Same as the first constructor, only without passing ownership of any
  // components to the circuit.
  Circuit(int num_inputs,
      std::vector<SubComponentEntry> sub_components,
      std::vector<PortIdentifier> outputs);

  virtual std::vector<Value> Eval(const std::vector<Value>& inputs) const;
  virtual int NumInputs() const;
  virtual int NumOutputs() const;

 private:
  const int num_inputs_;
  std::vector<SubComponentEntry> sub_components_;
  std::vector<PortIdentifier> outputs_;
  std::vector<std::unique_ptr<Component>> owned_components_;
};

#endif//CIRCUIT_H_
