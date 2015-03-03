
#ifndef STRUCT_EXPR_H_
#define STRUCT_EXPR_H_

#include <memory>
#include <vector>

#include "androcles/expr.h"
#include "androcles/struct_type.h"

class StructExpr : public Expr {
 public:
  StructExpr(const StructType* type, std::vector<std::unique_ptr<const Expr>> args);
  virtual ~StructExpr();
  virtual const StructType* GetType() const;

 private:
  const StructType* type_;
  std::vector<std::unique_ptr<const Expr>> args_;
};

#endif//STRUCT_EXPR_H_

