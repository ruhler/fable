
#include "androcles/union_type.h"

#include <string>
#include <vector>

UnionType::UnionType(const std::string& name, const std::vector<Field>& fields)
  : Type(name, fields)
{}

UnionType::~UnionType()
{}

