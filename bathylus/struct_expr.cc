
#include "bathylus/struct_expr.h"

StructExpr::StructExpr(const Type* type, const std::vector<const Expr*>& args)
  : type_(type), args_(args)
{}

StructExpr::~StructExpr()
{}

Value StructExpr::Eval(const std::unordered_map<std::string, Value>& env) const {
  std::vector<Value> args;
  for (auto& arg : args_) {
    args.push_back(arg->Eval(env));
  }
  return Value::Struct(type_, args);
}

