
#include "bathylus/access_expr.h"

AccessExpr::AccessExpr(const Expr* arg, int tag)
  : arg_(arg), tag_(tag)
{}

AccessExpr::~AccessExpr()
{}

Value AccessExpr::Eval(const std::unordered_map<std::string, Value>& env) const {
  Value arg = arg_->Eval(env);
  return arg.Access(tag_);
}

