
#ifndef STRUCT_EXPR_H_
#define STRUCT_EXPR_H_

#include <vector>

#include "androcles/expr.h"
#include "androcles/struct_type.h"

class StructExpr : public Expr {
 public:
  StructExpr(const StructType* type, const std::vector<const Expr*> args);
  virtual ~StructExpr();
  virtual const StructType* GetType() const;

 private:
  const StructType* type_;
  const std::vector<const Expr*> args_;
};

#endif//STRUCT_EXPR_H_

