
#include <memory>

#include <gtest/gtest.h>

#include "bathylus/case_expr.h"
#include "bathylus/expr.h"
#include "bathylus/function.h"
#include "bathylus/let_expr.h"
#include "bathylus/struct_expr.h"
#include "bathylus/type.h"
#include "bathylus/union_expr.h"
#include "bathylus/var_expr.h"

struct Heap {
 public:
  Expr* alloc(Expr* expr) {
    exprs_.push_back(std::unique_ptr<Expr>(expr));
    return expr;
  }

  Type* alloc(Type* type) {
    types_.push_back(std::unique_ptr<Type>(type));
    return type;
  }

  Function* alloc(Function* function) {
    functions_.push_back(std::unique_ptr<Function>(function));
    return function;
  }

 private:
  std::vector<std::unique_ptr<Expr>> exprs_;
  std::vector<std::unique_ptr<Function>> functions_;
  std::vector<std::unique_ptr<Type>> types_;
};

// Allocate a new object on the heap named 'heap' and return the result.
#define NEW(x) heap.alloc(new x)

TEST(BathylusTest, Basic) {
  Heap heap;

  Type* unit_t = NEW(Type(kStruct, "Unit", {}));
  Type* bit_t = NEW(Type(kUnion, "Bit", {{unit_t, "0"}, {unit_t, "1"}}));
  Type* full_adder_out_t = NEW(Type(kStruct, "FullAdderOut", {
        {bit_t, "z"},
        {bit_t, "cout"}
  }));

  Expr* a = NEW(VarExpr(bit_t, "a"));
  Expr* b = NEW(VarExpr(bit_t, "b"));
  Expr* cin = NEW(VarExpr(bit_t, "cin"));
  Expr* b0 = NEW(UnionExpr(bit_t, 0, NEW(StructExpr(unit_t, {}))));
  Expr* b1 = NEW(UnionExpr(bit_t, 1, NEW(StructExpr(unit_t, {}))));
  Expr* z = NEW(VarExpr(bit_t, "z"));
  Expr* cout = NEW(VarExpr(bit_t, "cout"));
  Function* adder1 = NEW(Function("FullAdder",
        {{bit_t, "a"}, {bit_t, "b"}, {bit_t, "cin"}}, full_adder_out_t,
        NEW(LetExpr({
            {bit_t, "z", NEW(CaseExpr(a, {
                NEW(CaseExpr(b, {cin, NEW(CaseExpr(cin, {b1, b0}))})),
                NEW(CaseExpr(b, {NEW(CaseExpr(cin, {b1, b0})), cin}))}))},
            {bit_t, "cout", NEW(CaseExpr(a, {
                NEW(CaseExpr(b, {b0, cin})),
                NEW(CaseExpr(b, {cin, b1}))}))}},
            NEW(StructExpr(full_adder_out_t, {z, cout}))))));
}

