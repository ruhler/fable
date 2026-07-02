/**
 * @file data.fble.test.c
 *  Unit test for data.fble.c functions.
 */

#include <assert.h>   // for assert
#include <string.h>   // for strcmp

#include <fble/fble-alloc.h>       // for FbleFree
#include <fble/fble-runtime.h>     // for FbleRuntime, etc.

#include "data.fble.h"    // For FbleNewStringValue, etc.

static void TestNewStringValue()
{
  FbleRuntime* runtime = FbleNewRuntime();

  // Sample string that cover various size utf8 multibyte characters.
  static const char* cstr = "aπ–😀.";
  FbleValue* str = FbleNewStringValue(runtime, cstr);

  // Read back the characters one by one to verify were decoded properly.
  uint32_t chars[5];
  for (size_t i = 0; i < 5; ++i) {
    assert(FbleUnionValueTag(str, 1) == 0);

    FbleValue* items = FbleUnionValueArg(str, 1);
    FbleValue* head = FbleStructValueField(items, 2, 0);
    chars[i] = FbleCharValueAccess(head);
    str = FbleStructValueField(items, 2, 1);
  }

  assert(FbleUnionValueTag(str, 1) == 1);

  assert(chars[0] == 0x61);
  assert(chars[1] == 0x03C0);
  assert(chars[2] == 0x2013);
  assert(chars[3] == 0x1F600);
  assert(chars[4] == 0x2E);

  FbleFreeRuntime(runtime);
}

static void TestStringValueAccess()
{
  FbleRuntime* runtime = FbleNewRuntime();

  // Sample string that cover various size utf8 multibyte characters.
  static const char* cstr = "aπ–😀.";
  FbleValue* str = FbleNewStringValue(runtime, cstr);
  char* ncstr = FbleStringValueAccess(str);
  assert(strcmp(cstr, ncstr) == 0);
  FbleFree(ncstr);

  FbleFreeRuntime(runtime);
}

int main()
{
  TestNewStringValue();
  TestStringValueAccess();
  return 0;
}
