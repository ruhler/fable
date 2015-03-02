
#ifndef UNION_EXPR_H_
#define UNION_EXPR_H_

#include <string>

#include "androcles/expr.h"

class UnionType;

class UnionExpr : public Expr {
 public:
  UnionExpr(const UnionType* type, const std::string& field, const Expr* value);
  virtual ~UnionExpr();
  virtual const Type* GetType() const;

 private:
  const UnionType* type_;
  const std::string field_;
  const Expr* value_;
};

#endif//UNION_EXPR_H_

