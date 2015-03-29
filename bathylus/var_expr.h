
#ifndef BATHYLUS_VAR_EXPR_H_
#define BATHYLUS_VAR_EXPR_H_

#include <string>

#include "bathylus/expr.h"

class Type;

class VarExpr : public Expr {
 public:
  VarExpr(const Type* type, const std::string& name);
  virtual ~VarExpr();
  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const;

 private:
  const Type* type_;
  std::string name_;
};

#endif//BATHYLUS_VAR_EXPR_H_
