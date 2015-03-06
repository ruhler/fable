
#include <iostream>

#include <gtest/gtest.h>

#include "androcles/type.h"

TEST(AndroclesTypeTest, Basic) {
  TypeEnv env;

  Type unit_t = env.DeclareStruct("unit_t", {});
  EXPECT_EQ(kStruct, unit_t.GetKind());
  EXPECT_EQ("unit_t", unit_t.GetName());
  EXPECT_EQ(0, unit_t.NumFields());
  EXPECT_EQ(Type::Null(), unit_t.TypeOfField("foo"));
  EXPECT_EQ(-1, unit_t.IndexOfField("foo"));
  EXPECT_TRUE(unit_t == unit_t);
  EXPECT_FALSE(unit_t != unit_t);

  Type bool_t = env.DeclareUnion("bool_t", {{unit_t, "true"}, {unit_t, "false"}});
  EXPECT_EQ(kUnion, bool_t.GetKind());
  EXPECT_EQ("bool_t", bool_t.GetName());
  EXPECT_EQ(2, bool_t.NumFields());
  EXPECT_EQ(unit_t, bool_t.TypeOfField("false"));
  EXPECT_EQ(1, bool_t.IndexOfField("false"));
  EXPECT_NE(unit_t, bool_t);

  Type mixed_t = env.DeclareStruct("mixed_t", {{unit_t, "unit"}, {bool_t, "bool"}});
  EXPECT_EQ(unit_t, mixed_t.TypeOfField("unit"));
  EXPECT_EQ(bool_t, mixed_t.TypeOfField("bool"));
  EXPECT_EQ(unit_t, mixed_t.TypeOfField(0));
  EXPECT_EQ(bool_t, mixed_t.TypeOfField(1));

  EXPECT_EQ(unit_t, env.LookupType("unit_t"));
  EXPECT_EQ(bool_t, env.LookupType("bool_t"));
  EXPECT_EQ(Type::Null(), env.LookupType("foo"));
}

