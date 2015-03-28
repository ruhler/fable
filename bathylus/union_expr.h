
#ifndef BATHYLUS_UNION_EXPR_H_
#define BATHYLUS_UNION_EXPR_H_

#include "bathylus/expr.h"

class Type;

class UnionExpr : public Expr {
 public:
  UnionExpr(const Type* type, int tag, const Expr* expr);
  virtual ~UnionExpr();

 private:
  const Type* type_;
  int tag_;
  const Expr* expr_;
};

#endif//BATHYLUS_UNION_EXPR_H_

