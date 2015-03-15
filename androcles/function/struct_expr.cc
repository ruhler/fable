
#include "androcles/function/struct_expr.h"

#include "error.h"

namespace androcles {
namespace function {

StructExpr::StructExpr(Type type, const std::vector<const Expr*>& args)
  : type_(type), args_(args)
{
  CHECK_EQ(kStruct, type.GetKind());
  CHECK_EQ(type.NumFields(), args.size());
  for (int i = 0; i < type.NumFields(); i++) {
    CHECK_EQ(type.TypeOfField(i), args[i]->GetType());
  }
}

StructExpr::~StructExpr()
{}

Type StructExpr::GetType() const {
  return type_;
}

Value StructExpr::Eval(const std::unordered_map<std::string, Value>& env) const {
  std::vector<Value> args;
  for (auto& arg : args_) {
    args.push_back(arg->Eval(env));
  }
  return Value::Struct(type_, args);
}

}  // namespace function
}  // namespace androcles

