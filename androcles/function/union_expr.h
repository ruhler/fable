
#ifndef ANDROCLES_FUNCTION_UNION_EXPR_H_
#define ANDROCLES_FUNCTION_UNION_EXPR_H_

#include <string>
#include <unordered_map>

#include "androcles/function/expr.h"
#include "androcles/value.h"
#include "androcles/type.h"

namespace androcles {
namespace function {

class UnionExpr : public Expr {
 public:
  UnionExpr(Type type, const std::string& field_name, const Expr* expr);
  virtual ~UnionExpr();

  virtual Type GetType() const;
  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const;

 private:
  Type type_;
  std::string field_name_;
  const Expr* expr_;
};

}  // namespace function
}  // namespace androcles

#endif//ANDROCLES_FUNCTION_UNION_EXPR_H_

