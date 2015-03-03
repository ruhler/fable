
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "androcles/decl_var_expr.h"
#include "androcles/field.h"
#include "androcles/struct_expr.h"
#include "androcles/struct_type.h"
#include "androcles/struct_value.h"
#include "androcles/union_expr.h"
#include "androcles/union_type.h"
#include "androcles/union_value.h"
#include "androcles/var_decl.h"

TEST(AndroclesTest, Basic) {
  TypeEnv type_env;

  // Unit
  Type unit_t = type_env.DeclareStruct("Unit", {});
  StructExpr unit_e(&unit_t, {});
  StructValue unit_v(&unit_t, {});

  // Bit
  Type bit_t = type_env.DeclareUnion("Bit", {{unit_t, "0"}, {unit_t, "1"}});
  UnionExpr b0_e(&bit_t, "0", &unit_e);
  UnionExpr b1_e(&bit_t, "1", &unit_e);
  UnionValue b0_v(&bit_t, "0", &unit_v);
  UnionValue b1_v(&bit_t, "1", &unit_v);

  Type full_adder_out_t = type_env.DeclareStruct("FullAdderOut", {
      {bit_t, "z"},
      {bit_t, "cout"}
  });

  VarDecl b0(&bit_t, "0", &b0_e);
  VarDecl b1(&bit_t, "1", &b1_e);

  InputVarExpr a_e(&bit_t, "a");
  InputVarExpr b_e(&bit_t, "b");
  InputVarExpr c_e(&bit_t, "c");
  VarDecl z(...);

  SelectExpr b_0_c(&b_e, {{"0", &b0_e}, {"1", &c_e}});
  SelectExpr b_c_1(&b_e, {{"0", &c_e}, {"1", &b1_e}});
  SelectExpr cout_e(&a_e, {{"0", &b_0_c}, {"1", &b_c_1}});
  VarDecl cout(&cout_e);

  StructExpr result_e(&full_adder_out_t,
      {DeclVarExpr(&z), DeclVarExpr(&cout)});
  Function full_adder("FullAdder",
      {{&bit_t, "a"}, {&bit_t, "b"}, {&bit_t, "cin"}},
      &full_adder_out_t,
      {&b0, &b1, &z, &cout},
      &result_e);

  EXPECT_EQ(StructValue(&full_adder_out_t, {"z", &b1_v}, {"cout", &b0_v}),
      full_adder.Eval({"a", &b0_v}, {"b", &b1_v}, {"c", &b0_v}));
  EXPECT_EQ(StructValue(&full_adder_out_t, {"z", &b0_v}, {"cout", &b1_v}),
      full_adder.Eval({"a", &b0_v}, {"b", &b1_v}, {"c", &b1_v}));
}

