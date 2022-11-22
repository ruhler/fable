
#include <stdio.h>
#include <string.h>

#include <fble/fble.h>

void HelloModule(FbleExecutableProgram* program);

FbleValue* ParseArg(FbleValueHeap* heap, const char* arg)
{
  if (strlen(arg) != 4) {
    fprintf(stderr, "illegal arg '%s': 4 bits required\n", arg);
    return NULL;
  }

  for (size_t i = 0; i < 4; ++i) {
    if (arg[i] != '0' && arg[i] != '1') {
      fprintf(stderr, "illegal arg '%s': 4 bits required\n", arg);
      return NULL;
    }
  }

  FbleValue* unit = FbleNewStructValue_(heap, 0);

  FbleValue* bit3 = FbleNewUnionValue(heap, arg[0] - '0', unit);
  FbleValue* bit2 = FbleNewUnionValue(heap, arg[1] - '0', unit);
  FbleValue* bit1 = FbleNewUnionValue(heap, arg[2] - '0', unit);
  FbleValue* bit0 = FbleNewUnionValue(heap, arg[3] - '0', unit);

  FbleValue* bits = FbleNewStructValue_(heap, 4, bit3, bit2, bit1, bit0);

  FbleReleaseValue(heap, unit);

  FbleReleaseValue(heap, bit3);
  FbleReleaseValue(heap, bit2);
  FbleReleaseValue(heap, bit1);
  FbleReleaseValue(heap, bit0);

  return bits;
}

int main(int argc, const char* argv[])
{
  FbleValueHeap* heap = FbleNewValueHeap();
  FbleValue* linked = FbleLinkFromCompiled(heap, HelloModule, NULL);

  if (linked == NULL) {
    FbleFreeValueHeap(heap);
    return 1;
  }

  FbleValue* and4 = FbleEval(heap, linked, NULL);
  FbleReleaseValue(heap, linked);

  if (and4 == NULL) {
    FbleFreeValueHeap(heap);
    return 1;
  }

  if (argc < 3) {
    fprintf(stderr, "usage: hello ARG1 ARG2\n");
    fprintf(stderr, "example: hello 0011 1010\n");
    return 1;
  }

  FbleValue* x = ParseArg(heap, argv[1]);
  FbleValue* y = ParseArg(heap, argv[2]);
  if (x == NULL || y == NULL) {
    FbleReleaseValue(heap, x);
    FbleReleaseValue(heap, y);
    FbleReleaseValue(heap, and4);
    FbleFreeValueHeap(heap);
    return 1;
  }

  FbleValue* args[] = { x, y };
  FbleValue* result = FbleApply(heap, and4, args, NULL);

  FbleReleaseValue(heap, x);
  FbleReleaseValue(heap, y);
  FbleReleaseValue(heap, and4);

  if (result == NULL) {
    FbleFreeValueHeap(heap);
    return 1;
  }

  printf("Result: ");
  for (size_t i = 0; i < 4; ++i) {
    FbleValue* bit = FbleStructValueAccess(result, i);
    printf("%c", FbleUnionValueTag(bit) == 0 ? '0' : '1');
  }
  printf("\n");

  FbleReleaseValue(heap, result);
  FbleFreeValueHeap(heap);
  return 0;
}
