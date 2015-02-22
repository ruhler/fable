
#include "androcles/struct_literal.h"

#include <vector>

#include "androcles/expr.h"
#include "androcles/struct_type.h"
#include "error.h"

StructLiteral::StructLiteral(const StructType& type, const std::vector<const Expr&> args)
  : type_(type), args_(args)
{}

StructLiteral::~StructLiteral()
{}

const Type& StructLiteral::GetType() const {
  return type_;
}

void StructLiteral::Verify(Verification& verification) const {
  // Verify all of the arguments
  for (const Expr& arg : args_) {
    arg.Verify(verification);
  }

  // Verify the right number of arguments were passed.
  const std::vector<Field>& fields = type_.GetFields();
  if (args_.size() != fields.size()) {
    verification.Fail()
      << "Wrong number of arguments to "
      << type_.GetName() << " struct literal. "
      << "Expected: " << fields.size()
      << ", but found: " << args_.size() << ".";
  }

  // Verify the arguments that were passed have the right type.
  for (int i = 0; i < args_.size() && i < fields.size(); i++) {
    const Type& type = args_[i]->GetType();
    if (type != fields[i].type) {
      verification.Fail()
        << "Type mismatch on the " << i << "th argument."
        << "Expected type " << fields[i].type << " for field "
        << fields[i].name << ", but found type " << type;
    }
  }
}

