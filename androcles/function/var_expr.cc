
#include "androcles/function/var_expr.h"

#include "error.h"

namespace androcles {
namespace function {

VarExpr::VarExpr(Type type, const std::string& name)
: type_(type), name_(name)
{}

VarExpr::~VarExpr()
{}

Type VarExpr::GetType() const {
  return type_;
}

Value VarExpr::Eval(const std::unordered_map<std::string, Value>& env) const {
  auto result = env.find(name_);
  CHECK(result != env.end()) << "Variable " << name_ << " not found in scope.";
  return result->second;
}

}  // namespace function
}  // namespace androcles

