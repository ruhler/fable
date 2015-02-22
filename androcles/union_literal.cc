
#include "union_literal.h"

#include <string>

#include "androcles/expr.h"
#include "androcles/union_type.h"
#include "androcles/verification.h"

UnionLiteral::UnionLiteral(const UnionType& type, const std::string& field, const Expr& value)
  : type_(type), field_(field), value_(value)
{}

const Type& UnionLiteral::GetType() const {
  return type_;
}

void UnionLiteral::Verify(Verification& verification) const {
  value_.Verify(verification);

  if (!type_.HasField(field_)) {

  }


}

