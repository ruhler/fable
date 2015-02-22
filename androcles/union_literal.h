
#ifndef UNION_LITERAL_H_
#define UNION_LITERAL_H_

#include "androcles/expr.h"

class Verification;

class UnionLiteral : public Expr {
 public:
  UnionLiteral(const UnionType& type, const std::string& field, const Expr& value);
  virtual ~UnionLiteral();
  virtual const Type& GetType() const;
  virtual void Verify(Verification& verification) const;

 private:
  const UnionType& type_;
  const std::string field_;
  const Expr& value_;
};

#endif//UNION_LITERAL_H_

