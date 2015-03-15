
#include "androcles/function/application_expr.h"

#include "error.h"

namespace androcles {
namespace function {

ApplicationExpr::ApplicationExpr(const Function* function,
                                 const std::vector<const Expr*>& args)
  : function_(function), args_(args)
{
  CHECK(function != nullptr);
  CHECK_EQ(function->NumArgs(), args_.size());
  for (int i = 0; i < args_.size(); i++) {
    CHECK_EQ(function->TypeOfArg(i), args_[i]->GetType());
  }
}

ApplicationExpr::~ApplicationExpr()
{}

Type ApplicationExpr::GetType() const {
  CHECK(function_ != nullptr);
  return function_->GetOutType();
}

Value ApplicationExpr::Eval(const std::unordered_map<std::string, Value>& env) const {
  CHECK(function_ != nullptr);

  std::vector<Value> args;
  for (auto& arg : args_) {
    args.push_back(arg->Eval(env));
  }
  return function_->Eval(args);
}

}  // namespace function
}  // namespace androcles

