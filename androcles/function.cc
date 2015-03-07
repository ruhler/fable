
#include "androcles/function.h"

#include "error.h"

class Expr_ { 
 public:
  virtual ~Expr_();

  virtual Type GetType() const = 0;
  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const = 0;
};

Expr::Expr(const Expr_* expr)
  : expr_(expr)
{}

Type Expr::GetType() const {
  return expr_->GetType();
}

Value Expr::Eval(const std::unordered_map<std::string, Value>& env) const {
  return expr_->Eval(env);
}

class VarExpr : public Expr_ {
 public:
  VarExpr(Type type, const std::string& name);
  virtual ~VarExpr();

  virtual Type GetType() const;
  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const;

 private:
  Type type_;
  std::string name_;
};

VarExpr::VarExpr(Type type, const std::string& name)
  : type_(type), name_(name)
{}

VarExpr::~VarExpr()
{}

Type VarExpr::GetType() const {
  return type_;
}

Value VarExpr::Eval(const std::unordered_map<std::string, Value>& env) const {
  auto result = env.find(name_);
  CHECK(result != env.end()) << "Variable " << name_ << " not found in scope.";
  return result->second;
}

class UnionExpr : public Expr_ {
 public:
  UnionExpr(Type type, const std::string& field_name, Expr expr);
  virtual ~UnionExpr();

  virtual Type GetType() const;
  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const;

 private:
  Type type_;
  std::string field_name_;
  Expr expr_;
};

UnionExpr::UnionExpr(Type type, const std::string& field_name, Expr expr)
  : type_(type), field_name_(field_name), expr_(expr)
{
  CHECK_EQ(kUnion, type.GetKind());
  CHECK_EQ(type.TypeOfField(field_name), expr.GetType());
}

UnionExpr::~UnionExpr()
{}

Type UnionExpr::GetType() {
  return type_;
}

Value UnionExpr::Eval(const std::unordered_map<std::string, Value>& env) const {
  Value value = expr_.Eval(env);
  return Value::Union(type_, field_name_, value);
}

class StructExpr : public Expr_ {
 public:
  StructExpr(Type type, const std::vector<Expr>& args);
  virtual ~StructExpr();

  virtual Type GetType() const;
  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const;

 private:
  Type type_;
  std::vector<Expr> args_;
};

StructExpr::StructExpr(Type type, const std::vector<Expr>& args)
  : type_(type), args_(args)
{
  CHECK_EQ(kStruct, type.GetKind());
  CHECK_EQ(type.NumFields(), args.size());
  for (int i = 0; i < type.NumFields(); i++) {
    CHECK_EQ(type.TypeOfField(i), args[i].GetType());
  }
}

StructExpr::~StructExpr()
{}

Type StructExpr::GetType() {
  return type_;
}

Value StructExpr::Eval(const std::unordered_map<std::string, Value>& env) const {
  std::vector<Value> args;
  for (auto& expr : args_) {
    args.push_back(expr_.Eval(env));
  }
  return Value::Struct(type_, args);
}

class SelectExpr : public Expr_ { 
 public:
  SelectExpr(Expr select, const std::vector<Alt>& alts);
  virtual ~SelectExpr();

  virtual Type GetType() const;
  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const;

 private:
  Expr select_;
  std::vector<Alt> alts_;
};

SelectExpr::SelectExpr(Expr select, const std::vector<Alt>& alts)
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

SelectExpr::~SelectExpr()
{}

Type SelectExpr::GetType() const {
  return alts_[0].value.GetType();
}

Value SelectExpr::Eval(const std::unordered_map<std::string, Value>& env) const {
  Value select = select_.Eval(env);
  int index = select.GetType().IndexOfField(select.GetTag());
  CHECK_GE(index, 0);
  CHECK_LT(index, alts_.size());
}

class AccessExpr : public Expr_ {
 public:
  AccessExpr(Expr source, const std::string& field_name);
  virtual ~AccessExpr();

  virtual Type GetType() const;
  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const;

 private:
  Expr source_;
  std::string field_name_;
};

AccessExpr::AccessExpr(Expr source, const std::string& field_name)
  : source_(source), field_name_(field_name)
{
  CHECK(source.GetType().HasField(field_name));
}

Type AccessExpr::GetType() const {
  return source.GetType().TypeOfField(field_name);
}

Value AccessExpr::Eval(const std::unordered_map<std::string, Value>& env) const {
  Value source = source_.Eval(env);
  return source.GetField(field_name);
}

AccessExpr::AccessExpr(Expr source, const std::string& field_name)
  : source_(source), field_name_(field_name)
{
  CHECK(source.GetType().HasField(field_name));
}

Type AccessExpr::GetType() const {
  return source.GetType().TypeOfField(field_name);
}

Value AccessExpr::Eval(const std::unordered_map<std::string, Value>& env) const {
  Value source = source_.Eval(env);
  return source.GetField(field_name);
}

Function::Function(const std::string& name,
    const std::vector<Field>& args,
    Type out_type)
  : name_(name), args_(args), out_type_(out_type), return_(Expr::Null())
{}

Function::~Function()
{}

Expr Var(const std::string& name) {
  for (auto& arg : args_) {
    if (arg.name == name) {
      return NewExpr(new VarExpr(arg.type, name));
    }
  }
  for (auto& decl : decls_) {
    if (decl.name == name) {
      return NewExpr(new VarExpr(decl.type, name));
    }
  }
  CHECK(false) << "No variable " << name << " in scope.";
}

Expr Function::Declare(Type type, const std::string& name, Expr expr) {
  for (auto& arg : args_) {
    CHECK(arg.name != name) << "Variable " << name << " shadows argument.";
  }
  for (auto& decl : decls_) {
    CHECK(decl.name != name) << "Variable " << name << " shadows variable.";
  }
  decls_.push_back({type, name, expr});
  return NewExpr(new VarExpr(type, name));
}

Expr Function::Union(Type type, const std::string& field_name, Expr value) {
  return NewExpr(new UnionExpr(type, field_name, value));
}

Expr Function::Struct(Type type, const std::vector<Expr> args) {
  return NewExpr(new StructExpr(type, args));
}

Expr Function::Select(Expr select, const std::vector<Alt>& alts) {
  return NewExpr(new SelectExpr(select, alts));
}

Expr Function::Access(Expr source, const std::string& field_name) {
  return NewExpr(new AccessExpr(source, field_name));
}

void Function::Return(Expr expr) {
  CHECK_EQ(Expr::Null(), expr);
  return_ = expr;
}

Value Function::Eval(const std::vector<Value>& args) const {
  CHECK_EQ(args_.size(), args.size());

  std::unordered_map env;
  for (int i = 0; i < args_.size(); i++) {
    auto result = env.emplace(args_[i].name, args[i]);
    CHECK(result.second) << "Duplicate arg named " << args_[i].name;
  }

  for (auto& decl : decls_) {
    auto result = env.emplace(decl.name, decl.value.Eval(env));
    CHECK(result.second) << "Duplicate assignment to " << decl.name;
  }
  return return_.Eval(env);
}

Expr Function::NewExpr(Expr_* expr) {
  exprs_.push_back(std::unique_ptr<Expr_>(expr));
  return Expr(expr);
}

