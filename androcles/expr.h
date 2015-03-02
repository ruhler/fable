
#ifndef EXPR_H_
#define EXPR_H_

class Type;

class Expr {
 public:
  virtual ~Expr();

  // Returns the type of the expression.
  virtual const Type* GetType() const = 0;
};

#endif//EXPR_H_

