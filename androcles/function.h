
#ifndef ANDROCLES_FUNCTION_H_
#define ANDROCLES_FUNCTION_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "androcles/type.h"
#include "androcles/value.h"
#include "androcles/function/expr.h"
#include "androcles/function/function.h"

namespace androcles {

class Expr {
 public:
  Type GetType() const;
  Value Eval(const std::unordered_map<std::string, Value>& env) const;

  // Returns a dummy Expr object for use as a sentinal value.
  static Expr Null();

 private:
  Expr(const function::Expr* expr);
  friend class FunctionBuilder;

  const function::Expr* expr_;
};

class Function {
 public:
  int NumArgs() const;
  Type TypeOfArg(int i) const;
  Type OutType() const;

  // Evaluate the function on the given arguments.
  Value Eval(const std::vector<Value>& args) const;

  // Returns a dummy Function object for use as a sentinal value.
  static Function Null();

 private:
  explicit Function(const function::Function* function);
  friend class FunctionBuilder;
  friend class FunctionEnv;

  const function::Function* function_;
};

struct Alt {
  std::string tag;
  Expr value;
};

class FunctionBuilder {
 public:
  // Create a function builder for a function with the given arguments and
  // output type.
  FunctionBuilder(const std::vector<Field>& args, Type out_type);

  // Clear and reset the builder to start building a new function.
  void Reset(const std::vector<Field>& args, Type out_type);

  // Returns an expression referring to the variable with the given name.
  // The name must match either an input variable or a declared variable.
  Expr Var(const std::string& name);

  // Declare a variable with the given type, name, and value.
  // Returns an Expr referring to the declared variable.
  Expr Declare(Type type, const std::string& name, Expr expr);

  // Returns a union literal expression.
  Expr Union(Type type, const std::string& field_name, Expr value);

  // Returns a struct literal expression.
  Expr Struct(Type type, const std::vector<Expr>& args);

  // Returns a select expression.
  Expr Select(Expr select, const std::vector<Alt>& alts);

  // Returns a field access expression.
  Expr Access(Expr source, const std::string& field_name);

  // Returns a function application expression.
  Expr Application(Function function, const std::vector<Expr>& args);

  // Defines the result of the function as the given expression.
  void Return(Expr expr);

  // Build the function and return the result.
  //
  // The Return value of the function must be set before calling Build.
  // The Build method puts the function builder in an invalid state, at
  // which point the Reset method must be called before using this
  // FunctionBuilder for building other functions.
  std::unique_ptr<function::Function> Build();

 private:
  // Helper function for allocating new expressions.
  // expr is a pointer to an expression allocated with new.
  Expr NewExpr(const function::Expr* expr);

  std::unique_ptr<function::Function> function_;
};

class FunctionEnv {
 public:
  // Declares and returns a function built by a FunctionBuilder.
  Function Declare(const std::string& name,
      std::unique_ptr<const function::Function> func);

  // Returns the declared function with the given name.
  // Returns Function::Null() if there is no such function declared.
  Function Lookup(const std::string& name);

 private:
  std::unordered_map<std::string, std::unique_ptr<const function::Function>> functions_;
};

}  // namespace androcles

#endif//ANDROCLES_FUNCTION_H_

