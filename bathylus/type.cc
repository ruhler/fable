
#include "bathylus/type.h"

Type::Type(Kind kind, const std::string& name, const std::vector<Field>& fields)
  : kind_(kind), name_(name), fields_(fields)
{}

