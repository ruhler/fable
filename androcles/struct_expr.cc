
#include "androcles/struct_expr.h"

#include <vector>

#include "androcles/expr.h"
#include "androcles/struct_type.h"

StructExpr::StructExpr(const StructType* type, const std::vector<const Expr*> args)
  : type_(type), args_(args)
{}

StructExpr::~StructExpr()
{}

const StructType* StructExpr::GetType() const {
  return type_;
}

