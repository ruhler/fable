
#ifndef ANDROCLES_FUNCTION_SELECT_EXPR_H_
#define ANDROCLES_FUNCTION_SELECT_EXPR_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "androcles/function/expr.h"

namespace androcles {
namespace function {

struct Alt {
  std::string tag;
  const Expr* value;
};

class SelectExpr : public Expr { 
 public:
  // The caller must ensure the select and alts exprs passed in will remain
  // valid for the lifetime of this SelectExpr object.
  SelectExpr(const Expr* select, const std::vector<Alt>& alts);
  virtual ~SelectExpr();

  virtual Type GetType() const;
  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const;

 private:
  const Expr* select_;
  std::vector<Alt> alts_;
};

}  // namespace function
}  // namespace androcles

#endif//ANDROCLES_FUNCTION_SELECT_EXPR_H_

