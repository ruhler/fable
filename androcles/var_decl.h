
#ifndef VAR_DECL_H_
#define VAR_DECL_H_

#include <string>

class Type;
class Expr;

class VarDecl {
 public:
  VarDecl(const Type* type, const std::string& name, const Expr* expr);
  ~VarDecl();

  const Type* GetType() const;

 private:
  const Type* type_;
  std::string name_;
  const Expr* expr_;
};

#endif//VAR_DECL_H_

