
#include "bathylus/struct_expr.h"

StructExpr::StructExpr(const Type* type, const std::vector<const Expr*>& args)
  : type_(type), args_(args)
{}

StructExpr::~StructExpr()
{}

