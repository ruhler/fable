
#include "androcles/function.h"

#include "error.h"

namespace androcles {

class Expr_ { 
 public:
  virtual ~Expr_()
  {}

  virtual Type GetType() const = 0;
  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const = 0;
};

class VarExpr : public Expr_ {
 public:
  VarExpr(Type type, const std::string& name)
    : type_(type), name_(name)
  {}

  virtual ~VarExpr()
  {}

  virtual Type GetType() const {
    return type_;
  }

  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const {
    auto result = env.find(name_);
    CHECK(result != env.end()) << "Variable " << name_ << " not found in scope.";
    return result->second;
  }

 private:
  Type type_;
  std::string name_;
};

class UnionExpr : public Expr_ {
 public:
  UnionExpr(Type type, const std::string& field_name, Expr expr)
    : type_(type), field_name_(field_name), expr_(expr)
  {
    CHECK_EQ(kUnion, type.GetKind());
    CHECK_EQ(type.TypeOfField(field_name), expr.GetType());
  }

  virtual ~UnionExpr()
  {}

  virtual Type GetType() const {
    return type_;
  }

  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const {
    Value value = expr_.Eval(env);
    return Value::Union(type_, field_name_, value);
  }

 private:
  Type type_;
  std::string field_name_;
  Expr expr_;
};

class StructExpr : public Expr_ {
 public:
  StructExpr(Type type, const std::vector<Expr>& args)
    : type_(type), args_(args)
  {
    CHECK_EQ(kStruct, type.GetKind());
    CHECK_EQ(type.NumFields(), args.size());
    for (int i = 0; i < type.NumFields(); i++) {
      CHECK_EQ(type.TypeOfField(i), args[i].GetType());
    }
  }

  virtual ~StructExpr()
  {}

  virtual Type GetType() const {
    return type_;
  }

  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const {
    std::vector<Value> args;
    for (auto& arg : args_) {
      args.push_back(arg.Eval(env));
    }
    return Value::Struct(type_, args);
  }

 private:
  Type type_;
  std::vector<Expr> args_;
};

class SelectExpr : public Expr_ { 
 public:
  SelectExpr(Expr select, const std::vector<Alt>& alts)
    : select_(select), alts_(alts)
  {
    Type select_type = select.GetType();
    CHECK_EQ(kUnion, select_type.GetKind());
    CHECK_EQ(select_type.NumFields(), alts.size());
    CHECK_GT(select_type.NumFields(), 0);

    Type result_type = alts[0].value.GetType();
    for (auto& alt : alts) {
      CHECK(select_type.HasField(alt.tag));
      CHECK_EQ(result_type, alt.value.GetType());
    }
  }

  virtual ~SelectExpr()
  {}

  virtual Type GetType() const {
    return alts_[0].value.GetType();
  }

  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const {
    Value select = select_.Eval(env);
    int index = select.GetType().IndexOfField(select.GetTag());
    CHECK_GE(index, 0);
    CHECK_LT(index, alts_.size());
    return alts_[index].value.Eval(env);
  }

 private:
  Expr select_;
  std::vector<Alt> alts_;
};

class AccessExpr : public Expr_ {
 public:
  AccessExpr(Expr source, const std::string& field_name)
    : source_(source), field_name_(field_name)
  {
    CHECK(source.GetType().HasField(field_name));
  }

  virtual ~AccessExpr()
  {}

  virtual Type GetType() const {
    return source_.GetType().TypeOfField(field_name_);
  }

  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const {
    Value source = source_.Eval(env);
    return source.GetField(field_name_);
  }

 private:
  Expr source_;
  std::string field_name_;
};

class ApplicationExpr : public Expr_ {
 public:
  ApplicationExpr(Function function, const std::vector<Expr>& args)
    : function_(function), args_(args)
  {
    CHECK_EQ(function.NumArgs(), args.size());
    for (int i = 0; i < args.size(); i++) {
      CHECK_EQ(function.TypeOfArg(i), args[i].GetType());
    }
  }

  virtual ~ApplicationExpr()
  {}

  virtual Type GetType() const {
    return function_.OutType();
  }

  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const {
    std::vector<Value> args;
    for (auto& arg : args_) {
      args.push_back(arg.Eval(env));
    }
    return function_.Eval(args);
  }

 private:
  Function function_;
  std::vector<Expr> args_;
};

Expr::Expr()
  : Expr(nullptr)
{}

Expr::Expr(const Expr_* expr)
  : expr_(expr)
{}

Type Expr::GetType() const {
  return expr_->GetType();
}

Value Expr::Eval(const std::unordered_map<std::string, Value>& env) const {
  return expr_->Eval(env);
}

Expr Expr::Null() {
  return Expr(nullptr);
}

FunctionEnv::FunctionDecl::FunctionDecl()
{}

FunctionEnv::FunctionDecl::~FunctionDecl()
{}

FunctionEnv::FunctionDecl::FunctionDecl(FunctionDecl&& rhs) {
  name = std::move(rhs.name);
  args = std::move(rhs.args);
  decls = std::move(rhs.decls);
  out_type = std::move(rhs.out_type);
  out_expr = std::move(rhs.out_expr);
}

Function::Function(const FunctionDecl* decl)
  : decl_(decl)
{}

int Function::NumArgs() const {
  CHECK(decl_ != nullptr);
  return decl_->args.size();
}

Type Function::TypeOfArg(int i) const {
  CHECK(decl_ != nullptr);
  CHECK_GE(i, 0);
  CHECK_LT(i, decl_->args.size());
  return decl_->args[i].type;
}

Type Function::OutType() const {
  CHECK(decl_ != nullptr);
  return decl_->out_type;
}

Value Function::Eval(const std::vector<Value>& args) const {
  CHECK(decl_ != nullptr);
  CHECK_EQ(decl_->args.size(), args.size());

  std::unordered_map<std::string, Value> env;
  for (int i = 0; i < decl_->args.size(); i++) {
    auto result = env.emplace(decl_->args[i].name, args[i]);
    CHECK(result.second) << "Duplicate arg named " << decl_->args[i].name;
  }

  for (auto& decl : decl_->decls) {
    auto result = env.emplace(decl.name, decl.value.Eval(env));
    CHECK(result.second) << "Duplicate assignment to " << decl.name;
  }
  return decl_->out_expr.Eval(env);
}

Function Function::Null() {
  return Function(nullptr);
}

FunctionBuilder::FunctionBuilder(const std::vector<Field>& args, Type out_type) {
  Reset(args, out_type);
}

void FunctionBuilder::Reset(const std::vector<Field>& args, Type out_type) {
  decl_.name.clear();
  decl_.args = args;
  decl_.decls.clear();
  decl_.out_type = out_type;
  decl_.out_expr = Expr::Null();
  decl_.exprs.clear();
}

Expr FunctionBuilder::Var(const std::string& name) {
  for (auto& arg : decl_.args) {
    if (arg.name == name) {
      return NewExpr(new VarExpr(arg.type, name));
    }
  }
  for (auto& decl : decl_.decls) {
    if (decl.name == name) {
      return NewExpr(new VarExpr(decl.type, name));
    }
  }
  CHECK(false) << "No variable " << name << " in scope.";
}

Expr FunctionBuilder::Declare(Type type, const std::string& name, Expr expr) {
  for (auto& arg : decl_.args) {
    CHECK(arg.name != name) << "Variable " << name << " shadows argument.";
  }
  for (auto& decl : decl_.decls) {
    CHECK(decl.name != name) << "Variable " << name << " shadows variable.";
  }
  decl_.decls.push_back({type, name, expr});
  return NewExpr(new VarExpr(type, name));
}

Expr FunctionBuilder::Union(Type type, const std::string& field_name, Expr value) {
  return NewExpr(new UnionExpr(type, field_name, value));
}

Expr FunctionBuilder::Struct(Type type, const std::vector<Expr>& args) {
  return NewExpr(new StructExpr(type, args));
}

Expr FunctionBuilder::Select(Expr select, const std::vector<Alt>& alts) {
  return NewExpr(new SelectExpr(select, alts));
}

Expr FunctionBuilder::Access(Expr source, const std::string& field_name) {
  return NewExpr(new AccessExpr(source, field_name));
}

Expr FunctionBuilder::Application(Function function, const std::vector<Expr>& args) {
  return NewExpr(new ApplicationExpr(function, args));
}

void FunctionBuilder::Return(Expr expr) {
  // TODO: Re-enable this CHECK after implementing the operator== for Expr.
  // CHECK_EQ(Expr::Null(), expr);
  decl_.out_expr = expr;
}

FunctionEnv::FunctionDecl&& FunctionBuilder::Build() {
  return std::move(decl_);
}

Expr FunctionBuilder::NewExpr(Expr_* expr) {
  decl_.exprs.push_back(std::unique_ptr<Expr_>(expr));
  return Expr(expr);
}

Function FunctionEnv::Declare(const std::string& name, FunctionDecl&& built) {
  auto result = decls_.emplace(name, FunctionDecl(std::move(built)));

  if (!result.second) {
    return Function::Null();
  }
  return Function(&(result.first->second));
}

Function FunctionEnv::Lookup(const std::string& name) {
  auto result = decls_.find(name);
  if (result == decls_.end()) {
    return Function::Null();
  }
  return Function(&(result->second));
}

}  // namespace androcles

