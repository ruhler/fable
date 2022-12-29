
#include <stdio.h>

#include <fble/fble.h>

int main(int argc, const char* argv[])
{
  FbleSearchPath* search_path = FbleNewSearchPath();
  FbleSearchPathAppend(search_path, ".");

  FbleModulePath* module_path = FbleParseModulePath("/Hello%");
  if (module_path == NULL) {
    fprintf(stderr, "Failed to parse module path.\n");
    return 1;
  }

  FbleValueHeap* heap = FbleNewValueHeap();
  FbleValue* linked = FbleLinkFromSource(heap, search_path, module_path, NULL);
  FbleFreeModulePath(module_path);
  FbleFreeSearchPath(search_path);

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
