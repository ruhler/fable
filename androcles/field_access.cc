
#include "androcles/field_access.h"

#include "androcles/type.h"

FieldAccess::FieldAccess(const Expr* expr, const std::string& field)
  : expr_(expr), field_(field)
{}

FieldAccess::~FieldAccess()
{}

const Type* FieldAccess::GetType() const {
  return expr_->GetType()->TypeOfField(field_);
}

