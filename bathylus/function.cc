
#include "bathylus/function.h"

Function::Function(const std::string& name, const std::vector<Field>& args,
      const Type* out_type, const Expr* body)
  : name_(name), args_(args), out_type_(out_type), body_(body)
{}

