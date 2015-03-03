
#include "androcles/struct_expr.h"

#include <memory>
#include <vector>

#include "androcles/expr.h"
#include "androcles/struct_type.h"

StructExpr::StructExpr(const StructType* type, std::vector<std::unique_ptr<const Expr>> args)
  : type_(type), args_(std::move(args))
{}

StructExpr::~StructExpr()
{}

const StructType* StructExpr::GetType() const {
  return type_;
}

