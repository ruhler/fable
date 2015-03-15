
#ifndef ANDROCLES_FUNCTION_STRUCT_EXPR_H_
#define ANDROCLES_FUNCTION_STRUCT_EXPR_H_

#include <unordered_map>
#include <vector>

#include "androcles/function/expr.h"
#include "androcles/type.h"
#include "androcles/value.h"

namespace androcles {
namespace function {

class StructExpr : public Expr {
 public:
  StructExpr(Type type, const std::vector<const Expr*>& args);
  virtual ~StructExpr();

  virtual Type GetType() const;
  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const;

 private:
  Type type_;
  std::vector<const Expr*> args_;
};

}  // namespace function
}  // namespace androcles

#endif//ANDROCLES_FUNCTION_STRUCT_EXPR_H_

