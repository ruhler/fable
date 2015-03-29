
#include <memory>

#include <gtest/gtest.h>

#include "bathylus/lib/stdlib.h"
#include "bathylus/lib/addition.h"
#include "bathylus/type.h"
#include "bathylus/value.h"

TEST(BathylusTest, Basic) {
  StdLib stdlib;
  FullAdder full_adder(stdlib);

  EXPECT_EQ(Value::Struct(full_adder.out_t, {stdlib.b1_v, stdlib.b0_v}),
      full_adder.function->Eval({stdlib.b0_v, stdlib.b1_v, stdlib.b0_v}));
  EXPECT_EQ(Value::Struct(full_adder.out_t, {stdlib.b0_v, stdlib.b1_v}),
      full_adder.function->Eval({stdlib.b0_v, stdlib.b1_v, stdlib.b1_v}));
}

