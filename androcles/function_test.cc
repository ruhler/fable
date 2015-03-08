
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "androcles/function.h"
#include "androcles/type.h"
#include "androcles/value.h"

TEST(AndroclesFunctionTest, Basic) {
  TypeEnv type_env;

  Type unit_t = type_env.DeclareStruct("Unit", {});
  Type bit_t = type_env.DeclareUnion("Bit", {{unit_t, "0"}, {unit_t, "1"}});
  Type full_adder_out_t = type_env.DeclareStruct("FullAdderOut", {
      {bit_t, "z"},
      {bit_t, "cout"}
  });

  Function adder("FullAdder",
      {{bit_t, "a"}, {bit_t, "b"}, {bit_t, "cin"}},
      full_adder_out_t);

  Expr a = adder.Var("a");
  Expr b = adder.Var("b");
  Expr cin = adder.Var("cin");
  Expr b0 = adder.Declare(bit_t, "0", adder.Union(bit_t, "0", adder.Struct(unit_t, {})));
  Expr b1 = adder.Declare(bit_t, "1", adder.Union(bit_t, "1", adder.Struct(unit_t, {})));
  Expr z = adder.Declare(bit_t, "z",
      adder.Select(a, {
        {"0", adder.Select(b, {
                {"0", cin},
                {"1", adder.Select(cin, {{"0", b1}, {"1", b0}})}})},
        {"1", adder.Select(b, {
                {"0", adder.Select(cin, {{"0", b1}, {"1", b0}})},
                {"1", cin}})}}));
  Expr cout = adder.Declare(bit_t, "cout",
      adder.Select(a, {
        {"0", adder.Select(b, {{"0", b0}, {"1", cin}})},
        {"1", adder.Select(b, {{"0", cin}, {"1", b1}})}}));
  adder.Return(adder.Struct(full_adder_out_t, {z, cout}));

  Value unit_v = Value::Struct(unit_t, {});
  Value b0_v = Value::Union(bit_t, "0", unit_v);
  Value b1_v = Value::Union(bit_t, "1", unit_v);

  EXPECT_EQ(Value::Struct(full_adder_out_t, {b1_v, b0_v}),
      adder.Eval({b0_v, b1_v, b0_v}));
  EXPECT_EQ(Value::Struct(full_adder_out_t, {b0_v, b1_v}),
      adder.Eval({b0_v, b1_v, b1_v}));
}

