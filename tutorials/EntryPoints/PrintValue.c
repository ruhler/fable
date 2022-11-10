
#include <stdio.h>

#include <fble/fble.h>

int main(int argc, const char* argv[])
{
  FbleSearchPath search_path;
  FbleVectorInit(search_path);
  FbleVectorAppend(search_path, ".");

  const char* module_path = "/Tutorial2a%";

  FbleValueHeap* heap = FbleNewValueHeap();
  FbleValue* linked = FbleLinkFromCompiledOrSource(heap, NULL, NULL, search_path, module_path);
  FbleVectorFree(search_path);

  if (linked == NULL) {
    FbleFreeValueHeap(heap);
    return 1;
  }

  FbleValue* result = FbleEval(heap, linked, NULL);
  FbleReleaseValue(heap, linked);

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
