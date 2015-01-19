
#include <sstream>
#include <stdexcept>
#include <gtest/gtest.h>

#include "char_stream.h"
#include "parse_exception.h"
#include "token_stream.h"

#define NO_PARSE_EXCEPTION(x) try { x; } catch (ParseException& e) { FAIL() << e; }

TEST(TokenStreamTest, Basic) {
  std::istringstream iss("foo(a,b;c) {\n 00: 12;\n}");
  TokenStream stream(CharStream("test", iss));

  NO_PARSE_EXCEPTION(EXPECT_TRUE(stream.TokenIs(kWord)));
  NO_PARSE_EXCEPTION(EXPECT_EQ("foo", stream.GetWord()));

  NO_PARSE_EXCEPTION(EXPECT_TRUE(stream.TokenIs(kOpenParen)));
  NO_PARSE_EXCEPTION(stream.EatToken(kOpenParen));
  NO_PARSE_EXCEPTION(EXPECT_EQ("a", stream.GetWord()));
  NO_PARSE_EXCEPTION(stream.EatToken(kComma));
  NO_PARSE_EXCEPTION(EXPECT_EQ("b", stream.GetWord()));
  NO_PARSE_EXCEPTION(stream.EatToken(kSemicolon));
  NO_PARSE_EXCEPTION(EXPECT_EQ("c", stream.GetWord()));
  NO_PARSE_EXCEPTION(stream.EatToken(kCloseParen));
  NO_PARSE_EXCEPTION(stream.EatToken(kSpace));
  NO_PARSE_EXCEPTION(stream.EatToken(kOpenBrace));
  NO_PARSE_EXCEPTION(stream.EatSpace());
  NO_PARSE_EXCEPTION(EXPECT_EQ("00", stream.GetWord()));
  NO_PARSE_EXCEPTION(stream.EatToken(kColon));
  NO_PARSE_EXCEPTION(stream.EatSpace());
  NO_PARSE_EXCEPTION(stream.EatSpace());
  NO_PARSE_EXCEPTION(EXPECT_EQ("12", stream.GetWord()));
  NO_PARSE_EXCEPTION(stream.EatToken(kSemicolon));
  NO_PARSE_EXCEPTION(stream.EatToken(kSpace));
  NO_PARSE_EXCEPTION(stream.EatToken(kCloseBrace));
  NO_PARSE_EXCEPTION(EXPECT_TRUE(stream.TokenIs(kEndOfStream)));
  NO_PARSE_EXCEPTION(stream.EatToken(kEndOfStream));
  NO_PARSE_EXCEPTION(EXPECT_TRUE(stream.TokenIs(kEndOfStream)));
}

