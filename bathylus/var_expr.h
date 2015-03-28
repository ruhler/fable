
#ifndef BATHYLUS_VAR_EXPR_H_
#define BATHYLUS_VAR_EXPR_H_

#include <string>

#include "bathylus/expr.h"

class Type;

class VarExpr : public Expr {
 public:
  VarExpr(const Type* type, const std::string& name);
  virtual ~VarExpr();

 private:
  const Type* type_;
  std::string name_;
};

#endif//BATHYLUS_VAR_EXPR_H_
