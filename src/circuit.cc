
#include "circuit.h"

#include <cassert>

Circuit::Circuit(int num_inputs,
    std::vector<SubComponentEntry> sub_components,
    std::vector<PortIdentifier> outputs)
  : num_inputs_(num_inputs), sub_components_(sub_components), outputs_(outputs)
{
  // TODO: Pull the verification of correctness out into helper functions that
  // return a boolean rather than repeating code and directly asserting here.
  for (int i = 0; i < sub_components_.size(); i++) {
    SubComponentEntry& entry = sub_components[i];
    assert(entry.component->NumInputs() == entry.inputs.size()
      && "Wrong number of inputs for component");
    for (int j = 0; j < entry.inputs.size(); j++) {
      PortIdentifier& portid = entry.inputs[j];
      if (portid.component_index == kInputPortComponentIndex) {
        assert(portid.port_index >= 0 && portid.port_index < num_inputs
            && "Invalid port index for input");
      } else {
        assert(portid.component_index >= 0
            && portid.component_index < i
            && "Invalid port identifier component index");
        const Component* component = sub_components[portid.component_index].component;
        assert(portid.port_index >= 0
            && portid.port_index < component->NumOutputs()
            && "Invalid port identifier port index");
      }
    }
  }

  for (int j = 0; j < outputs.size(); j++) {
    PortIdentifier& portid = outputs[j];
    if (portid.component_index == kInputPortComponentIndex) {
      assert(portid.port_index >= 0 && portid.port_index < num_inputs
          && "Invalid port index for input");
    } else {
      assert(portid.component_index >= 0
          && portid.component_index < j
          && "Invalid port identifier component index");
      const Component* component = sub_components[portid.component_index].component;
      assert(portid.port_index >= 0
          && portid.port_index < component->NumOutputs()
          && "Invalid port identifier port index");
    }
  }
}

std::vector<Value> Circuit::Eval(const std::vector<Value>& inputs) const
{
  assert(inputs.size() == num_inputs_
    && "Wrong number of inputs given to circuit");

  // Edges will contain the outputs of all sub components. We compute this in
  // order of the sub components, with the guarantee from the constructor that
  // all components only refer to outputs of previews components in the list.
  // To avoid a special case, we set the first entry of edges to the input
  // values, which places the outputs of sub component i at index i+1.
  std::vector<std::vector<Value>> edges(sub_components_.size()+1);
  edges[0] = inputs;

  // TODO: Write a function to collect the inputs for a module, and reuse that
  // here and to collect the final outputs rather than duplicating code.
  for (int i = 0; i < sub_components_.size(); i++) {
    const SubComponentEntry& entry = sub_components_[i];
    std::vector<Value> sub_inputs;
    for (int j = 0; j < entry.inputs.size(); j++) {
      const PortIdentifier& portid = entry.inputs[j];
      sub_inputs.push_back(edges[portid.component_index+1][portid.port_index]);
    }
    edges[i+1] = entry.component->Eval(sub_inputs);
  } 

  std::vector<Value> outputs;
  for (int j = 0; j < outputs_.size(); j++) {
    const PortIdentifier& portid = outputs_[j];
    outputs.push_back(edges[portid.component_index+1][portid.port_index]);
  }
  return outputs;
}

int Circuit::NumInputs() const
{
  return num_inputs_;
}

int Circuit::NumOutputs() const
{
  return outputs_.size();
}

