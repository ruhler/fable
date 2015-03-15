
#include "androcles/function.h"

#include "androcles/function/access_expr.h"
#include "androcles/function/application_expr.h"
#include "androcles/function/function.h"
#include "androcles/function/select_expr.h"
#include "androcles/function/struct_expr.h"
#include "androcles/function/union_expr.h"
#include "androcles/function/var_expr.h"

#include "error.h"

namespace androcles {

using function::AccessExpr;
using function::ApplicationExpr;
using function::SelectExpr;
using function::StructExpr;
using function::UnionExpr;
using function::VarExpr;

Expr::Expr(const function::Expr* expr)
  : expr_(expr)
{}

Type Expr::GetType() const {
  CHECK(expr_ != nullptr) << "GetType called on Expr::Null()";
  return expr_->GetType();
}

Value Expr::Eval(const std::unordered_map<std::string, Value>& env) const {
  CHECK(expr_ != nullptr) << "Eval called on Expr::Null()";
  return expr_->Eval(env);
}

Expr Expr::Null() {
  return Expr(nullptr);
}

Function::Function(const function::Function* function)
  : function_(function)
{}

int Function::NumArgs() const {
  CHECK(function_ != nullptr) << "NumArgs called on Function::Null()";
  return function_->NumArgs();
}

Type Function::TypeOfArg(int i) const {
  CHECK(function_ != nullptr) << "TypeOfArg called on Function::Null()";
  return function_->TypeOfArg(i);
}

Type Function::OutType() const {
  CHECK(function_ != nullptr) << "OutType called on Function::Null()";
  return function_->GetOutType();
}

Value Function::Eval(const std::vector<Value>& args) const {
  CHECK(function_ != nullptr) << "Eval called on Function::Null()";
  return function_->Eval(args);
}

Function Function::Null() {
  return Function(nullptr);
}

FunctionBuilder::FunctionBuilder(const std::vector<Field>& args, Type out_type) {
  Reset(args, out_type);
}

void FunctionBuilder::Reset(const std::vector<Field>& args, Type out_type) {
  function_.reset(new function::Function(args, out_type));
}

Expr FunctionBuilder::Var(const std::string& name) {
  CHECK(function_.get() != nullptr);

  Type type = function_->TypeOfArg(name);
  if (type != Type::Null()) {
    return NewExpr(new VarExpr(type, name));
  }

  type = function_->TypeOfVar(name);
  if (type != Type::Null()) {
    return NewExpr(new VarExpr(type, name));
  }

  CHECK(false) << "No variable " << name << " in scope.";
}

Expr FunctionBuilder::Declare(Type type, const std::string& name, Expr expr) {
  CHECK(!function_->HasArg(name)) << "Variable " << name << " shadows argument.";
  CHECK(!function_->HasVar(name)) << "Variable " << name << " shadows variable.";
  function_->DeclareVar(type, name, expr.expr_);
  return NewExpr(new VarExpr(type, name));
}

Expr FunctionBuilder::Union(Type type, const std::string& field_name, Expr value) {
  return NewExpr(new UnionExpr(type, field_name, value.expr_));
}

Expr FunctionBuilder::Struct(Type type, const std::vector<Expr>& args) {
  std::vector<const function::Expr*> unboxed_args;
  for (auto& expr : args) {
    unboxed_args.push_back(expr.expr_);
  }
  return NewExpr(new StructExpr(type, unboxed_args));
}

Expr FunctionBuilder::Select(Expr select, const std::vector<Alt>& alts) {
  std::vector<function::Alt> unboxed_alts;
  for (auto& alt : alts) {
    unboxed_alts.push_back(function::Alt({alt.tag, alt.value.expr_}));
  }
  return NewExpr(new SelectExpr(select.expr_, unboxed_alts));
}

Expr FunctionBuilder::Access(Expr source, const std::string& field_name) {
  return NewExpr(new AccessExpr(source.expr_, field_name));
}

Expr FunctionBuilder::Application(Function function, const std::vector<Expr>& args) {
  std::vector<const function::Expr*> unboxed_args;
  for (auto& expr : args) {
    unboxed_args.push_back(expr.expr_);
  }
  return NewExpr(new ApplicationExpr(function.function_, unboxed_args));
}

void FunctionBuilder::Return(Expr expr) {
  // TODO: Re-enable this CHECK after implementing the operator== for Expr.
  // CHECK_EQ(Expr::Null(), expr);
  function_->SetOutExpr(expr.expr_);
}

std::unique_ptr<function::Function> FunctionBuilder::Build() {
  return std::move(function_);
}

Expr FunctionBuilder::NewExpr(const function::Expr* expr) {
  function_->Own(std::unique_ptr<const function::Expr>(expr));
  return Expr(expr);
}

Function FunctionEnv::Declare(const std::string& name, std::unique_ptr<const function::Function> built) {
  auto result = functions_.emplace(name, std::move(built));

  if (!result.second) {
    return Function::Null();
  }
  return Function(result.first->second.get());
}

Function FunctionEnv::Lookup(const std::string& name) {
  auto result = functions_.find(name);
  if (result == functions_.end()) {
    return Function::Null();
  }
  return Function(result->second.get());
}

}  // namespace androcles

