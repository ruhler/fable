
#include "circuit/circuit.h"

#include "error.h"

Component::~Component()
{}

int Component::NumInputs() const
{
  return Inputs().size();
}

int Component::NumOutputs() const
{
  return Outputs().size();
}

Circuit::Circuit(
    std::vector<std::string> inputs,
    std::vector<std::string> outputs,
    std::vector<SubComponentEntry> sub_components,
    std::vector<PortIdentifier> outvals,
    std::vector<std::unique_ptr<Component>> owned_components)
  : inputs_(inputs), outputs_(outputs), sub_components_(sub_components),
    outvals_(outvals), owned_components_(std::move(owned_components))
{
  CHECK_EQ(outputs.size(), outvals_.size())
    << "Number of output ports doesn't match the number of actual outputs";

  // TODO: Pull the verification of correctness out into helper functions that
  // return a boolean rather than repeating code and directly asserting here.
  for (int i = 0; i < sub_components_.size(); i++) {
    SubComponentEntry& entry = sub_components[i];
    CHECK_EQ(entry.component->NumInputs(), entry.inputs.size())
      << "Wrong number of inputs for component";
    for (int j = 0; j < entry.inputs.size(); j++) {
      PortIdentifier& portid = entry.inputs[j];
      if (portid.component_index == kInputPortComponentIndex) {
        CHECK_GE(portid.port_index, 0) << "Invalid port index for input.";
        CHECK_LT(portid.port_index, inputs.size())
          << "Invalid port index for input.";
      } else {
        CHECK_GE(portid.component_index, 0)
          << "Invalid port identifier component index.";
        CHECK_LT(portid.component_index, i)
          << "Invalid port identifier component index.";

        const Component* component = sub_components[portid.component_index].component;
        CHECK_GE(portid.port_index, 0)
          << "Invalid port identifier port index.";
        CHECK_LT(portid.port_index, component->NumOutputs())
          << "Invalid port identifier port index";
      }
    }
  }

  for (int j = 0; j < outvals.size(); j++) {
    PortIdentifier& portid = outvals[j];
    if (portid.component_index == kInputPortComponentIndex) {
      CHECK_GE(portid.port_index, 0) << "Invalid port index for input.";
      CHECK_LT(portid.port_index, inputs.size())
        << "Invalid port index for input.";
    } else {
      CHECK_GE(portid.component_index, 0)
        << "Invalid port identifier component index.";
      CHECK_LT(portid.component_index, outvals.size())
        << "Invalid port identifier component index.";
      const Component* component = sub_components[portid.component_index].component;
      CHECK_GE(portid.port_index, 0) << "Invalid port identifier port index.";
      CHECK_LT(portid.port_index, component->NumOutputs())
        << "Invalid port identifier port index.";
    }
  }
}

Circuit::Circuit(
    std::vector<std::string> inputs,
    std::vector<std::string> outputs,
    std::vector<SubComponentEntry> sub_components,
    std::vector<PortIdentifier> outvals)
  : Circuit(inputs, outputs, sub_components, outvals,
      std::vector<std::unique_ptr<Component>>())
{}

std::vector<Value> Circuit::Eval(const std::vector<Value>& inputs) const
{
  CHECK_EQ(inputs.size(), inputs_.size())
    << "Wrong number of inputs given to circuit";

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
  for (int j = 0; j < outvals_.size(); j++) {
    const PortIdentifier& portid = outvals_[j];
    outputs.push_back(edges[portid.component_index+1][portid.port_index]);
  }
  return outputs;
}

std::vector<std::string> Circuit::Inputs() const
{
  return inputs_;
}

std::vector<std::string> Circuit::Outputs() const
{
  return outputs_;
}

