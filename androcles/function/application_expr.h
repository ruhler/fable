
#ifndef ANDROCLES_FUNCTION_APPLICATION_EXPR_H_
#define ANDROCLES_FUNCTION_APPLICATION_EXPR_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "androcles/function/expr.h"
#include "androcles/function/function.h"
#include "androcles/type.h"
#include "androcles/value.h"

namespace androcles {
namespace function {

class ApplicationExpr : public Expr {
 public:
  // The caller must ensure the function and args passed in will remain valid
  // for the lifetime of this ApplicationExpr object.
  ApplicationExpr(const Function* function, const std::vector<const Expr*>& args);
  virtual ~ApplicationExpr();

  virtual Type GetType() const;
  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const;

 private:
  const Function* function_;
  std::vector<const Expr*> args_;
};

}  // namespace function
}  // namespace androcles

#endif//ANDROCLES_FUNCTION_APPLICATION_EXPR_H_

