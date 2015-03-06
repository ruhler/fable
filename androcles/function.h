
#ifndef ANDROCLES_FUNCTION_H_
#define ANDROCLES_FUNCTION_H_

#include <string>
#include <vector>

#include "androcles/type.h"
#include "androcles/value.h"

class Expr_;

class Expr {
 public:
  Type GetType() const;

 private:
  Expr(const Expr_* expr);
  const Expr_* expr_;
};

struct Alt {
  std::string tag;
  Expr value;
};

class Function {
 public:
  Function(const std::string& name, 
      const std::vector<Field>& args,
      Type out_type);
  Function::~Function();

  // Returns an expression referring to the variable with the given name.
  // The name must match either an input variable or a declared variable.
  Expr Var(const std::string& name);

  // Declare a variable with the given type, name, and value.
  // Returns an Expr referring to the declared variable.
  Expr Declare(Type type, const std::string& name, Expr expr);

  // Returns a union literal expression.
  Expr Union(Type type, const std::string& field_name, Expr value);

  // Returns a struct literal expression.
  Expr Struct(Type type, const std::vector<Expr> args);

  // Returns a select expression.
  Expr Select(Expr select, const std::vector<Alt>& alts);

  // Returns a field access expression.
  Expr Access(Expr source, const std::string& field_name);

  // Returns a function application expression.
  Expr Application(Expr function, const std::vector<Expr>& args);

  // Defines the result of the function as the given expression.
  void Return(Expr expr);

  // Evaluate the function on the given arguments.
  Value Eval(const std::vector<Value>& args) const;

 private:
  const std::string name_;
  const std::vector<Field> fields_;
  Type out_type_;
  Expr return_;

  // Ownership for all expressions created through this function.
  std::vector<std::unique_ptr<Expr_>> exprs_;
};

#endif//ANDROCLES_FUNCTION_H_

