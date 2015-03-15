
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "androcles/function.h"
#include "androcles/type.h"
#include "androcles/value.h"

namespace androcles {

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
  {
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
  }

  Function adder1 = functions.Declare("FullAdder", builder.Build());

  Value unit_v = Value::Struct(unit_t, {});
  Value b0_v = Value::Union(bit_t, "0", unit_v);
  Value b1_v = Value::Union(bit_t, "1", unit_v);

  EXPECT_EQ(Value::Struct(full_adder_out_t, {b1_v, b0_v}),
      adder1.Eval({b0_v, b1_v, b0_v}));
  EXPECT_EQ(Value::Struct(full_adder_out_t, {b0_v, b1_v}),
      adder1.Eval({b0_v, b1_v, b1_v}));

  Type bit4_t = types.DeclareStruct("Bit4", {
      {bit_t, "0"}, {bit_t, "1"}, {bit_t, "2"}, {bit_t, "3"}
  });

  Type adder_out_t = types.DeclareStruct("AdderOut", {
      {bit4_t, "z"}, {bit_t, "cout"}
  });

  builder.Reset({{bit4_t, "a"}, {bit4_t, "b"}, {bit_t, "cin"}}, adder_out_t);
  {
    Expr a = builder.Var("a");
    Expr b = builder.Var("b");
    Expr cin = builder.Var("cin");
    Expr x0 = builder.Declare(full_adder_out_t, "x0",
        builder.Application(adder1, {
          builder.Access(a, "0"),
          builder.Access(b, "0"),
          cin}));
    Expr x1 = builder.Declare(full_adder_out_t, "x1",
        builder.Application(adder1, {
          builder.Access(a, "1"),
          builder.Access(b, "1"),
          builder.Access(x0, "cout")}));
    Expr x2 = builder.Declare(full_adder_out_t, "x2",
        builder.Application(adder1, {
          builder.Access(a, "2"),
          builder.Access(b, "2"),
          builder.Access(x1, "cout")}));
    Expr x3 = builder.Declare(full_adder_out_t, "x3",
        builder.Application(adder1, {
          builder.Access(a, "3"),
          builder.Access(b, "3"),
          builder.Access(x2, "cout")}));
    builder.Return(builder.Struct(adder_out_t, {
          builder.Struct(bit4_t, {
            builder.Access(x0, "z"),
            builder.Access(x1, "z"),
            builder.Access(x2, "z"),
            builder.Access(x3, "z"),
            }),
          builder.Access(x3, "cout")
          }));
  }

  Function adder4 = functions.Declare("Adder", builder.Build());

  // Try 2 + 6 = 8
  // Note: Bits are listed with least significant on the left.
  EXPECT_EQ(Value::Struct(adder_out_t, {
        Value::Struct(bit4_t, {b0_v, b0_v, b0_v, b1_v}),
        b0_v}),
      adder4.Eval({
        Value::Struct(bit4_t, {b0_v, b1_v, b0_v, b0_v}),
        Value::Struct(bit4_t, {b0_v, b1_v, b1_v, b0_v}),
        b0_v}));
}

}  // namespace androcles

