
#ifndef ANDROCLES_FUNCTION_H_
#define ANDROCLES_FUNCTION_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "androcles/type.h"
#include "androcles/value.h"

namespace androcles {

class Expr_;

class FunctionEnv {
 public:
  class Expr {
   public:
    Expr();
    Expr(const Expr_* expr);
    Type GetType() const;
    Value Eval(const std::unordered_map<std::string, Value>& env) const;

    // Expr object used to indicate when something goes wrong.
    static Expr Null();

   private:
    const Expr_* expr_;
  };

  struct Alt {
    std::string tag;
    Expr value;
  };

 private:
  struct VarDecl {
    Type type;
    std::string name;
    Expr value;
  };

  struct FunctionDecl {
    FunctionDecl();
    ~FunctionDecl();
    FunctionDecl(const FunctionDecl& rhs) = delete;
    FunctionDecl(FunctionDecl&& rhs);

    std::string name;
    std::vector<Field> args;
    std::vector<VarDecl> decls;
    Type out_type;
    Expr out_expr;

    // Ownership for all expressions used by this function.
    std::vector<std::unique_ptr<Expr_>> exprs;
  };

 public:
  class Function {
   public:
    Function(const FunctionDecl* decl);

    int NumArgs() const;
    Type TypeOfArg(int i) const;
    Type OutType() const;

    // Evaluate the function on the given arguments.
    Value Eval(const std::vector<Value>& args) const;

    static Function Null();

   private:
    const FunctionDecl* decl_;
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
    FunctionDecl&& Build();

   private:
    // Helper function for allocating new expressions.
    // expr is a pointer to an expression allocated with new.
    // This takes over ownership of the expression by adding it to the exprs_
    // list, and returning an 'Expr' object for it.
    Expr NewExpr(Expr_* expr);

    FunctionDecl decl_;
  };

  // Declares and returns a function built by a FunctionBuilder.
  Function Declare(const std::string& name, FunctionDecl&& built);

  // Returns the declared function with the given name.
  // Returns Function::Null() if there is no such function declared.
  Function Lookup(const std::string& name);

 private:
  std::unordered_map<std::string, FunctionDecl> decls_;
};

typedef FunctionEnv::Alt Alt;
typedef FunctionEnv::Expr Expr;
typedef FunctionEnv::Function Function;
typedef FunctionEnv::FunctionBuilder FunctionBuilder;

}  // namespace androcles

#endif//ANDROCLES_FUNCTION_H_

