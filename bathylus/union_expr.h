
#ifndef BATHYLUS_UNION_EXPR_H_
#define BATHYLUS_UNION_EXPR_H_

#include "bathylus/expr.h"

class Type;

class UnionExpr : public Expr {
 public:
  UnionExpr(const Type* type, int tag, const Expr* expr);
  virtual ~UnionExpr();
  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const;

 private:
  const Type* type_;
  int tag_;
  const Expr* expr_;
};

#endif//BATHYLUS_UNION_EXPR_H_

