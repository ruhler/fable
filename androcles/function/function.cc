
#include "androcles/function/function.h"

#include "androcles/function/expr.h"
#include "error.h"

namespace androcles {
namespace function {

Function::Function(const std::vector<Field>args, Type out_type)
  : args_(args), out_type_(out_type), out_expr_(nullptr)
{}

const std::string& Function::GetName() const {
  return name_;
}

void Function::SetName(const std::string& name) {
  name_ = name;
}

int Function::NumArgs() const {
  return args_.size();
}

const std::string& Function::NameOfArg(int i) const {
  CHECK_GE(i, 0);
  CHECK_LT(i, args_.size());
  return args_[i].name;
}

Type Function::TypeOfArg(int i) const {
  CHECK_GE(i, 0);
  CHECK_LT(i, args_.size());
  return args_[i].type;
}

bool Function::HasArg(const std::string& name) const {
  for (auto& field : args_) {
    if (field.name == name) {
      return true;
    }
  }
  return false;
}

Type Function::TypeOfArg(const std::string& name) const {
  for (auto& field : args_) {
    if (field.name == name) {
      return field.type;
    }
  }
  return Type::Null();
}

Type Function::GetOutType() const {
  return out_type_;
}

void Function::SetOutExpr(const Expr* out_expr) {
  out_expr_ = out_expr;
}

void Function::DeclareVar(Type type, const std::string& name, const Expr* value) {
  vars_.push_back(VarDecl({type, name, value}));
}

bool Function::HasVar(const std::string& name) const {
  for (auto& var : vars_) {
    if (var.name == name) {
      return true;
    }
  }
  return false;
}

Type Function::TypeOfVar(const std::string& name) const {
  for (auto& var : vars_) {
    if (var.name == name) {
      return var.type;
    }
  }
  return Type::Null();
}

Value Function::Eval(const std::vector<Value>& args) const {
  CHECK_EQ(args_.size(), args.size());

  std::unordered_map<std::string, Value> env;
  for (int i = 0; i < args_.size(); i++) {
    auto result = env.emplace(args_[i].name, args[i]);
    CHECK(result.second) << "Duplicate arg named " << args_[i].name;
  }

  for (auto& decl : vars_) {
    auto result = env.emplace(decl.name, decl.value->Eval(env));
    CHECK(result.second) << "Duplicate assignment to " << decl.name;
  }
  return out_expr_->Eval(env);
}

void Function::Own(std::unique_ptr<const Expr> expr) {
  owned_exprs_.push_back(std::move(expr));
}

}  // namespace function
}  // namespace androcles

