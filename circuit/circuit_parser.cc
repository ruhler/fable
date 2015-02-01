
#include "circuit/circuit_parser.h"

#include <memory>
#include <unordered_map>
#include <vector>

#include "error.h"
#include "circuit/truth_table_component.h"
#include "parser/common_parser.h"
#include "parser/parse_exception.h"
#include "parser/token_stream.h"
#include "truth_table/truth_table_parser.h"

// TODO: This whole implementation may be cleaner if we define an internal
// class to give access to common variables, such as inputs, instances, and
// tokens. Rather than passing around those variables everywhere.

static std::unique_ptr<Component> ParseComponent(SpaceEatingTokenStream& tokens)
{
  Location location = tokens.GetLocation();
  std::string word = tokens.GetWord();
  if (word == "Circuit") {
    return std::unique_ptr<Component>(
        new Circuit(ParseCircuitAfterMagic(tokens)));
  }
  
  if (word == "TruthTable") {
    return std::unique_ptr<Component>(
        new TruthTableComponent(ParseTruthTableAfterMagic(tokens)));
  }

  throw ParseException(location) << "Unknown component type: " << word;
}

// Parse a single input value of either the form 'A' or the form 'foo.X'.
// * inputs is the list of inputs to the overall circuit
// * instances is the list of instances defined so far
// * instances_by_name maps name of instances defined so far to their entries
// in the instances list.
// Throws ParseException if there is a parse error.
Circuit::PortIdentifier ParseInputVal(
    const std::vector<std::string>& inputs, 
    const std::vector<Circuit::SubComponentEntry>& instances,
    const std::unordered_map<std::string, int>& instances_by_name,
    SpaceEatingTokenStream& tokens)
{
  Circuit::PortIdentifier port;
  Location location = tokens.GetLocation();
  std::string source = tokens.GetWord();
  if (tokens.TokenIs(kPeriod)) {
    // In this case, the source is an output from some instance.
    auto instance = instances_by_name.find(source);
    if (instance == instances_by_name.end()) {
      throw ParseException(location) << source << ": no such instance.";
    }
    port.component_index = instance->second;

    tokens.EatToken(kPeriod);
    location = tokens.GetLocation();
    std::string field = tokens.GetWord();
    port.port_index = instances[instance->second].component->OutputByName(field);
    if (port.port_index < 0) {
      throw ParseException(location) << field
        << " is not a valid output of component " << source;
    }
    return port;
  }

  // In this case, the source is an input to the circuit. Find out which input
  // it is.
  port.component_index = Circuit::kInputPortComponentIndex;
  for (int i = 0; i < inputs.size(); i++) {
    if (inputs[i] == source) {
      port.port_index = i;
      return port;
    }
  }
  throw ParseException(location) << "Unknown circuit input: " << source;
}

// Parse input vals of the form '(A, b.Z, ...)'
// * inputs is the list of inputs to the overall circuit
// * instances is the list of instances defined so far
// * instances_by_name maps name of instances defined so far to their entries
// in the instances list.
// Throws ParseException if there is a parse error.
std::vector<Circuit::PortIdentifier> ParseInputVals(
    const std::vector<std::string>& inputs, 
    const std::vector<Circuit::SubComponentEntry>& instances,
    const std::unordered_map<std::string, int>& instances_by_name,
    SpaceEatingTokenStream& tokens)
{
  tokens.EatToken(kOpenParen);
  std::vector<Circuit::PortIdentifier> ports;
  if (tokens.TokenIs(kWord)) {
    ports.push_back(ParseInputVal(inputs, instances, instances_by_name, tokens));
  }

  while (!tokens.TokenIs(kCloseParen)) {
    tokens.EatToken(kComma);
    ports.push_back(ParseInputVal(inputs, instances, instances_by_name, tokens));
  }
  tokens.EatToken(kCloseParen);
  return ports;
}

Circuit ParseCircuit(std::string source, std::istream& istream)
{
  Location location;
  SpaceEatingTokenStream tokens(source, istream);

  location = tokens.GetLocation();
  std::string word = tokens.GetWord();
  if (word != "Circuit") {
    throw ParseException(location)
      << "Expected the word 'Circuit', but found '" << word << "'.";
  }
  return ParseCircuitAfterMagic(tokens);
}

Circuit ParseCircuitAfterMagic(SpaceEatingTokenStream& tokens)
{
  tokens.EatToken(kOpenParen);
  std::vector<std::string> inputs = ParseInputs(tokens);
  std::vector<std::string> outputs = ParseOutputs(tokens);
  tokens.EatToken(kOpenBrace);

  Location location = tokens.GetLocation();
  std::string word = tokens.GetWord();

  // Read in the component definitions.
  // * components is a list of all the compoments loaded.
  // * components_by_name maps the name of the component to its value.
  std::vector<std::unique_ptr<Component>> components;
  std::unordered_map<std::string, Component*> components_by_name;
  while (word == "Component") {
    location = tokens.GetLocation();
    std::string name = tokens.GetWord();
    if (components_by_name.count(name) != 0) {
      throw ParseException(location) << "Duplicate components named " << name;
    }
    tokens.EatToken(kColon);
    std::unique_ptr<Component> component = ParseComponent(tokens);
    tokens.EatToken(kSemicolon);
    components_by_name.insert({name, component.get()});
    components.push_back(std::move(component));

    location = tokens.GetLocation();
    word = tokens.GetWord();
  }

  // Read in the instance definitions.
  // * instances_by_name maps the name of an instance to its entry in the 
  // instances list.
  std::vector<Circuit::SubComponentEntry> instances;
  std::unordered_map<std::string, int> instances_by_name;
  while (word == "Instance") {
    location = tokens.GetLocation();
    std::string name = tokens.GetWord();
    if (instances_by_name.count(name) != 0) {
      throw ParseException(location) << "Duplicate instances named " << name;
    }
    tokens.EatToken(kColon);
    location = tokens.GetLocation();
    std::string component_name = tokens.GetWord();
    auto component = components_by_name.find(component_name);
    if (component == components_by_name.end()) {
      throw ParseException(location) << component_name << ": no such component";
    }
    Circuit::SubComponentEntry entry;
    entry.component = component->second;
    entry.inputs = ParseInputVals(inputs, instances, instances_by_name, tokens);
    instances_by_name.insert({name, instances.size()});
    instances.push_back(entry);
    tokens.EatToken(kSemicolon);

    location = tokens.GetLocation();
    word = tokens.GetWord();
  }

  if (word != "Output") {
    ParseException exception(location);
    exception << "Expected the word ";
    if (instances.empty()) {
      if (components.empty()) {
        exception << "'Component' or ";
      }
      exception << "'Instance' or ";
    }
    exception << "'Output', but found '" << word << "'.";
    throw exception;
  }
  std::vector<Circuit::PortIdentifier> outvals = ParseInputVals(
      inputs, instances, instances_by_name, tokens);
  tokens.EatToken(kSemicolon);
  tokens.EatToken(kCloseBrace);
  return Circuit(inputs, outputs, instances, outvals, std::move(components));
}

