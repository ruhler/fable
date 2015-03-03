
#include "androcles/field_access.h"

#include <memory>

#include "androcles/type.h"

FieldAccess::FieldAccess(std::unique_ptr<const Expr> expr, const std::string& field)
  : expr_(std::move(expr)), field_(field)
{}

FieldAccess::~FieldAccess()
{}

const Type* FieldAccess::GetType() const {
  return expr_->GetType()->TypeOfField(field_);
}

