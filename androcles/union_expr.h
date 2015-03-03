
#ifndef UNION_EXPR_H_
#define UNION_EXPR_H_

#include <memory>
#include <string>

#include "androcles/expr.h"

class UnionType;

class UnionExpr : public Expr {
 public:
  UnionExpr(const UnionType* type, const std::string& field, std::unique_ptr<const Expr> value);
  virtual ~UnionExpr();
  virtual const Type* GetType() const;

 private:
  const UnionType* type_;
  const std::string field_;
  std::unique_ptr<const Expr> value_;
};

#endif//UNION_EXPR_H_

