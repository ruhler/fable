
#include <sstream>
#include <gtest/gtest.h>

#include "char_stream.h"

TEST(CharStreamTest, Basic) {
  std::istringstream istream("he llo\nwo\nrld");
  CharStream char_stream("test", istream);
  Location expected_location;
  expected_location.source = "test";
  expected_location.line = 1;
  expected_location.column = 1;

  EXPECT_EQ(expected_location, char_stream.GetLocation());

  // PeekChar should read the head character, without changing the state of
  // the stream.
  EXPECT_EQ('h', char_stream.PeekChar());
  EXPECT_EQ('h', char_stream.PeekChar());
  EXPECT_EQ(expected_location, char_stream.GetLocation());

  // GetChar should read the head character, and advance to the next.
  EXPECT_EQ('h', char_stream.GetChar());
  EXPECT_EQ('e', char_stream.PeekChar());
  expected_location.column = 2;
  EXPECT_EQ(expected_location, char_stream.GetLocation());

  // Verify we don't do funny stuff with space.
  EXPECT_EQ('e', char_stream.GetChar());
  EXPECT_EQ(' ', char_stream.GetChar());
  expected_location.column = 4;
  EXPECT_EQ(expected_location, char_stream.GetLocation());

  // Reading a newline should bring us to a newline.
  EXPECT_EQ('l', char_stream.GetChar());
  EXPECT_EQ('l', char_stream.GetChar());
  EXPECT_EQ('o', char_stream.GetChar());
  expected_location.column = 7;
  EXPECT_EQ(expected_location, char_stream.GetLocation());
  EXPECT_EQ('\n', char_stream.GetChar());
  expected_location.line = 2;
  expected_location.column = 1;
  EXPECT_EQ(expected_location, char_stream.GetLocation());

  // Reading multiple lines should cause no problem.
  EXPECT_EQ('w', char_stream.GetChar());
  EXPECT_EQ('o', char_stream.GetChar());
  EXPECT_EQ('\n', char_stream.GetChar());
  EXPECT_EQ('r', char_stream.GetChar());
  EXPECT_EQ('l', char_stream.GetChar());
  expected_location.line = 3;
  expected_location.column = 3;
  EXPECT_EQ(expected_location, char_stream.GetLocation());

  // Verify we get the end of file indicator.
  EXPECT_EQ('d', char_stream.GetChar());
  EXPECT_EQ(EOF, char_stream.GetChar());
  EXPECT_EQ(EOF, char_stream.GetChar());
  EXPECT_EQ(EOF, char_stream.PeekChar());
  expected_location.line = 3;
  expected_location.column = 4;
  EXPECT_EQ(expected_location, char_stream.GetLocation());
}

