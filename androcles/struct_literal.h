
#ifndef STRUCT_LITERAL_H_
#define STRUCT_LITERAL_H_

#include "androcles/expr.h"

class Verification;

class StructLiteral : public Expr {
 public:
  StructLiteral(const StructType& type, const std::vector<const Expr&> args);
  virtual ~StructLiteral();
  virtual const Type& GetType() const;
  virtual void Verify(Verification& verification) const;

 private:
  const StructType& type_;
  const std::vector<const Expr&> args_;
};

#endif//STRUCT_LITERAL_H_

