
#ifndef BATHYLUS_STRUCT_EXPR_H_
#define BATHYLUS_STRUCT_EXPR_H_

#include <vector>

#include "bathylus/expr.h"

class Type;

class StructExpr : public Expr {
 public:
  StructExpr(const Type* type, const std::vector<const Expr*>& args);
  virtual ~StructExpr();

 private:
  const Type* type_;
  std::vector<const Expr*> args_;
};


#endif//BATHYLUS_STRUCT_EXPR_H_

