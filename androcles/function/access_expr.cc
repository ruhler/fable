
#include "androcles/function/access_expr.h"

#include "error.h"

namespace androcles {
namespace function {

AccessExpr::AccessExpr(const Expr* source, const std::string& field_name)
  : source_(source), field_name_(field_name)
{
  CHECK(source != nullptr);
  CHECK(source->GetType().HasField(field_name));
}

AccessExpr::~AccessExpr()
{}

Type AccessExpr::GetType() const {
  CHECK(source_ != nullptr);
  return source_->GetType().TypeOfField(field_name_);
}

Value AccessExpr::Eval(const std::unordered_map<std::string, Value>& env) const {
  CHECK(source_ != nullptr);
  Value source = source_->Eval(env);
  return source.GetField(field_name_);
}

}  // namespace function
}  // namespace androcles

