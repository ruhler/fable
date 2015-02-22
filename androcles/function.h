
#ifndef FUNCTION_H_
#define FUNCTION_H_

struct Arg {
  const Type* type;
  const std::string name;
};

struct VDecl {
  const Type* type;
  const std::string name;
  const Expr* value;
}

class Function {
 private:
  const std::string name_;
  const std::vector<Arg> args_;
  const Type* outtype_;
  const std::vector<VDecl> vdecls_;
  const Expr* return_;
};

#endif//FUNCTION_H_

