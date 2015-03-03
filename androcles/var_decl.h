
#ifndef VAR_DECL_H_
#define VAR_DECL_H_

#include <memory>
#include <string>

class Type;
class Expr;

class VarDecl {
 public:
  VarDecl(const Type* type, const std::string& name, std::unique_ptr<const Expr> expr);
  ~VarDecl();

  const Type* GetType() const;

 private:
  const Type* type_;
  std::string name_;
  std::unique_ptr<const Expr> expr_;
};

#endif//VAR_DECL_H_

