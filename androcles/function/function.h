
#ifndef ANDROCLES_FUNCTION_FUNCTION_H_
#define ANDROCLES_FUNCTION_FUNCTION_H_

#include <string>
#include <vector>

#include "androcles/type.h"
#include "androcles/value.h"

namespace androcles {
namespace function {

class Expr;

class Function {
 public:
  Function(const std::vector<Field> args, Type out_type);

  const std::string& GetName() const;
  void SetName(const std::string& name);

  // Returns the number of arguments the function takes.
  int NumArgs() const;

  // Returns the name of the ith argument, starting at 0.
  // This requires that 0 <= i < NumArgs().
  const std::string& NameOfArg(int i) const;

  // Returns the type of the ith argument, starting at 0.
  // This requires that 0 <= i < NumArgs().
  Type TypeOfArg(int i) const;

  // Returns true if the function has an argument with the given name.
  bool HasArg(const std::string& name) const;

  // Returns the type of the arg with the given name.
  // Returns Type::Null() if there is no argument with that name.
  Type TypeOfArg(const std::string& name) const;

  // Returns the type of value output by the function.
  Type GetOutType() const;

  // Sets the return value of this function to the given expression.
  // The expression must remain valid for the lifetime of the function object.
  void SetOutExpr(const Expr* out_expr);

  // Add a local variable declaration.
  void DeclareVar(Type type, const std::string& name, const Expr* value);

  // Returns true if the function has a local variable with the given name.
  bool HasVar(const std::string& name) const;

  // Returns the type of the declared variable with the given name.
  // Returns Type::Null() if there is no argument with that name.
  Type TypeOfVar(const std::string& name) const;

  // Evaluates the function on the given arguments.
  Value Eval(const std::vector<Value>& args) const;

  // Passes ownership of the given expression to the Function object.
  void Own(std::unique_ptr<const Expr> expr);

 private:
  struct VarDecl {
    Type type;
    std::string name;
    const Expr* value;
  };

  std::string name_;
  std::vector<Field> args_;
  Type out_type_;
  std::vector<VarDecl> vars_;
  const Expr* out_expr_;

  std::vector<std::unique_ptr<const Expr>> owned_exprs_;
};

}  // namespace function
}  // namespace androcles

#endif//ANDROCLES_FUNCTION_FUNCTION_H_

