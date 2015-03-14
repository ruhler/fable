
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "androcles/function.h"
#include "androcles/type.h"
#include "androcles/value.h"

TEST(AndroclesFunctionTest, Basic) {
  TypeEnv types;

  Type unit_t = types.DeclareStruct("Unit", {});
  Type bit_t = types.DeclareUnion("Bit", {{unit_t, "0"}, {unit_t, "1"}});
  Type full_adder_out_t = types.DeclareStruct("FullAdderOut", {
      {bit_t, "z"},
      {bit_t, "cout"}
  });

  FunctionEnv functions;

  FunctionBuilder builder({{bit_t, "a"}, {bit_t, "b"}, {bit_t, "cin"}}, full_adder_out_t);

  Expr a = builder.Var("a");
  Expr b = builder.Var("b");
  Expr cin = builder.Var("cin");
  Expr b0 = builder.Declare(bit_t, "0", builder.Union(bit_t, "0", builder.Struct(unit_t, {})));
  Expr b1 = builder.Declare(bit_t, "1", builder.Union(bit_t, "1", builder.Struct(unit_t, {})));
  Expr z = builder.Declare(bit_t, "z",
      builder.Select(a, {
        {"0", builder.Select(b, {
                {"0", cin},
                {"1", builder.Select(cin, {{"0", b1}, {"1", b0}})}})},
        {"1", builder.Select(b, {
                {"0", builder.Select(cin, {{"0", b1}, {"1", b0}})},
                {"1", cin}})}}));
  Expr cout = builder.Declare(bit_t, "cout",
      builder.Select(a, {
        {"0", builder.Select(b, {{"0", b0}, {"1", cin}})},
        {"1", builder.Select(b, {{"0", cin}, {"1", b1}})}}));
  builder.Return(builder.Struct(full_adder_out_t, {z, cout}));

  Function adder = functions.Declare("FullAdder", builder.Build());

  Value unit_v = Value::Struct(unit_t, {});
  Value b0_v = Value::Union(bit_t, "0", unit_v);
  Value b1_v = Value::Union(bit_t, "1", unit_v);

  EXPECT_EQ(Value::Struct(full_adder_out_t, {b1_v, b0_v}),
      adder.Eval({b0_v, b1_v, b0_v}));
  EXPECT_EQ(Value::Struct(full_adder_out_t, {b0_v, b1_v}),
      adder.Eval({b0_v, b1_v, b1_v}));
}

