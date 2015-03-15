
#ifndef ANDROCLES_FUNCTION_VAR_EXPR_H_
#define ANDROCLES_FUNCTION_VAR_EXPR_H_

#include <string>
#include <unordered_map>

#include "androcles/function/expr.h"
#include "androcles/type.h"

namespace androcles {
namespace function {

class VarExpr : public Expr {
 public:
  VarExpr(Type type, const std::string& name);
  virtual ~VarExpr();

  virtual Type GetType() const;
  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const;

 private:
  Type type_;
  std::string name_;
};

}  // namespace function
}  // namespace androcles

#endif//ANDROCLES_FUNCTION_VAR_EXPR_H_

