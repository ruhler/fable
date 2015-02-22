
#ifndef EXPR_H_
#define EXPR_H_

class Type;
class Verification;

class Expr {
 public:
  virtual ~Expr();

  // Returns the type of the expression.
  virtual const Type& GetType() const = 0;

  // Verifies the expression is well formed.
  // The results of verification are reported to the verification argument.
  //
  // It is assumed any types involved have already been verified.
  virtual void Verify(Verification& verification) const = 0;
};

#endif//EXPR_H_

