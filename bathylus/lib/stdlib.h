
#ifndef BATHYLUS_LIB_STDLIB_H_
#define BATHYLUS_LIB_STDLIB_H_

#include "bathylus/expr.h"
#include "bathylus/function.h"
#include "bathylus/type.h"
#include "bathylus/struct_expr.h"
#include "bathylus/union_expr.h"
#include "bathylus/value.h"

class StdLib {
 public:
  StdLib();

  const Type* unit_t;
  const Expr* unit_e;
  Value unit_v;

  const Type* bit_t;
  const Expr* b0_e;
  const Expr* b1_e;
  Value b0_v;
  Value b1_v;

 private:
  Type unit_t_;
  StructExpr unit_e_;

  Type bit_t_;
  UnionExpr b0_e_;
  UnionExpr b1_e_;
};

#endif//BATHYLUS_LIB_STDLIB_H_
