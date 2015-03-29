
#include "bathylus/var_expr.h"

#include "error.h"

VarExpr::VarExpr(const Type* type, const std::string& name)
  : type_(type), name_(name)
{}

VarExpr::~VarExpr()
{}

Value VarExpr::Eval(const std::unordered_map<std::string, Value>& env) const {
  auto result = env.find(name_);
  CHECK(result != env.end()) << "Variable " << name_ << " not found in scope.";
  return result->second;
}

