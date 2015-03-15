
#include "androcles/function/union_expr.h"

#include "error.h"

namespace androcles {
namespace function {

UnionExpr::UnionExpr(Type type, const std::string& field_name, const Expr* expr)
  : type_(type), field_name_(field_name), expr_(expr)
{
  CHECK_EQ(kUnion, type.GetKind());
  CHECK_EQ(type.TypeOfField(field_name), expr->GetType());
}

UnionExpr::~UnionExpr()
{}

Type UnionExpr::GetType() const {
  return type_;
}

Value UnionExpr::Eval(const std::unordered_map<std::string, Value>& env) const {
  Value value = expr_->Eval(env);
  return Value::Union(type_, field_name_, value);
}

}  // namespace function
}  // namespace androcles

