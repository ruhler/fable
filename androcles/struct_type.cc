
#include "androcles/struct_type.h"

#include <string>
#include <vector>

#include "androcles/field.h"

StructType::StructType(const std::string& name, const std::vector<Field>& fields)
  : Type(name, fields)
{}

StructType::~StructType()
{}

