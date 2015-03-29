
#include "bathylus/union_expr.h"

UnionExpr::UnionExpr(const Type* type, int tag, const Expr* expr)
  : type_(type), tag_(tag), expr_(expr)
{}

UnionExpr::~UnionExpr()
{}

Value UnionExpr::Eval(const std::unordered_map<std::string, Value>& env) const {
  return Value::Union(type_, tag_, expr_->Eval(env));
}

