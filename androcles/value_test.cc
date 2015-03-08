
#include <gtest/gtest.h>

#include "androcles/type.h"
#include "androcles/value.h"

TEST(AndroclesValueTest, Undefined) {
  TypeEnv env;
  Type unit_t = env.DeclareStruct("unit_t", {});
  Type bool_t = env.DeclareUnion("bool_t", {{unit_t, "true"}, {unit_t, "false"}});

  Value undefined = Value::Undefined(bool_t);
  EXPECT_EQ(bool_t, undefined.GetType());
  EXPECT_TRUE(undefined.IsPartiallyUndefined());
  EXPECT_TRUE(undefined.IsCompletelyUndefined());
  EXPECT_TRUE(undefined.GetField("true").IsCompletelyUndefined());

  Value undefined2 = undefined;
  EXPECT_EQ(bool_t, undefined.GetType());
  EXPECT_TRUE(undefined.IsPartiallyUndefined());
  EXPECT_TRUE(undefined.IsCompletelyUndefined());
  EXPECT_TRUE(undefined.GetField("true").IsCompletelyUndefined());
  EXPECT_EQ(bool_t, undefined2.GetType());
  EXPECT_TRUE(undefined2.IsPartiallyUndefined());
  EXPECT_TRUE(undefined2.IsCompletelyUndefined());
  EXPECT_TRUE(undefined2.GetField("true").IsCompletelyUndefined());
}

TEST(AndroclesValueTest, Basic) {
  TypeEnv env;
  Type unit_t = env.DeclareStruct("unit_t", {});
  Type bool_t = env.DeclareUnion("bool_t", {{unit_t, "true"}, {unit_t, "false"}});
  Type enum_t = env.DeclareUnion("enum_t", {{unit_t, "a"}, {unit_t, "b"}, {unit_t, "c"}});

  Value unit_v = Value::Struct(unit_t, {});
  EXPECT_EQ(unit_t, unit_v.GetType());
  EXPECT_FALSE(unit_v.IsPartiallyUndefined());
  EXPECT_FALSE(unit_v.IsCompletelyUndefined());
  EXPECT_EQ(unit_v, unit_v);
  EXPECT_EQ(unit_v, Value::Struct(unit_t, {}));

  Value true_v = Value::Union(bool_t, "true", unit_v);
  EXPECT_EQ(bool_t, true_v.GetType());
  EXPECT_FALSE(true_v.IsPartiallyUndefined());
  EXPECT_FALSE(true_v.IsCompletelyUndefined());
  EXPECT_EQ("true", true_v.GetTag());
  EXPECT_EQ(true_v, true_v);
  EXPECT_NE(unit_v, true_v);

  Value false_v = Value::Union(bool_t, "false", unit_v);
  EXPECT_EQ("false", false_v.GetTag());
  EXPECT_NE(true_v, false_v);

  Value a_v = Value::Union(enum_t, "a", unit_v);
  Value b_v = Value::Union(enum_t, "b", unit_v);
  Value c_v = Value::Union(enum_t, "c", unit_v);
  EXPECT_EQ("c", true_v.Select({c_v, a_v}).GetTag());
  EXPECT_EQ("a", false_v.Select({c_v, a_v}).GetTag());
}

