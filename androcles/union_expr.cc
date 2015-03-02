
#include "union_expr.h"

#include <string>

#include "androcles/expr.h"
#include "androcles/union_type.h"

UnionExpr::UnionExpr(const UnionType* type, const std::string& field, const Expr* value)
  : type_(type), field_(field), value_(value)
{}

const Type* UnionExpr::GetType() const {
  return type_;
}

