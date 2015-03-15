
#ifndef ANDROCLES_FUNCTION_ACCESS_EXPR_H_
#define ANDROCLES_FUNCTION_ACCESS_EXPR_H_

#include "androcles/function/expr.h"

namespace androcles {
namespace function {

class AccessExpr : public Expr {
 public:
  AccessExpr(const Expr* source, const std::string& field_name);
  virtual ~AccessExpr();

  virtual Type GetType() const;
  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const;

 private:
  const Expr* source_;
  std::string field_name_;
};

}  // namespace function
}  // namespace androcles

#endif//ANDROCLES_FUNCTION_ACCESS_EXPR_H_

