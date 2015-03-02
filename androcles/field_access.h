
#ifndef FIELD_ACCESS_H_
#define FIELD_ACCESS_H_

#include <string>

#include "androcles/expr.h"

class FieldAccess : public Expr {
 public:
  FieldAccess(const Expr* expr, const std::string& field);
  virtual ~FieldAccess();
  virtual const Type* GetType() const;

 private:
  const Expr* expr_;
  const std::string field_;
};

#endif//FIELD_ACCESS_H_

