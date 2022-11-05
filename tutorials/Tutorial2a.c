
#include <fble/fble.h>

#define EX_SUCCESS 0
#define EX_COMPILE_ERROR 1
#define EX_RUNTIME_ERROR 2
#define EX_USAGE_ERROR 3
#define EX_OTHER_ERROR 4

static void PrintUsage(FILE* stream)
{
  fprintf(stream, "Usage: tutorial2a [OPTION...] -m MODULE_PATH\n"
      "\n"
      "Description:\n"
      "  Evaluates an fble program of type /Tutorial2a%%.Bit4@ and prints the\n"
      "  result.\n"
      "\n"
      "Options:\n"
      "  -h, --help\n"
      "     Print this help message and exit.\n"
      "  -I DIR\n"
      "     Adds DIR to the module search path.\n"
      "  -m, --module MODULE_PATH\n"
      "     The path of the module to get dependencies for.\n"
      "\n"
      "Exit Status:\n"
      "  %i on success.\n"
      "  %i on compile error.\n"
      "  %i on runtime error.\n"
      "  %i on usage error.\n"
      "  %i on other error.\n"
      "\n"
      "Example:\n"
      "  tutorial2a -I tutorials -m /Tutorial2a%%\n",
        EX_SUCCESS, EX_COMPILE_ERROR, EX_RUNTIME_ERROR,
        EX_USAGE_ERROR, EX_OTHER_ERROR);
}

int main(int argc, const char* argv[])
{
  FbleSearchPath search_path;
  FbleVectorInit(search_path);
  const char* module_path = NULL;
  bool help = false;
  bool error = false;

  argc--;
  argv++;
  while (!error && argc > 0) {
    if (FbleParseBoolArg("-h", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--help", &help, &argc, &argv, &error)) continue;
    if (FbleParseSearchPathArg(&search_path, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("-m", &module_path, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--module", &module_path, &argc, &argv, &error)) continue;
    if (FbleParseInvalidArg(&argc, &argv, &error)) continue;
  }

  if (help) {
    PrintUsage(stdout);
    FbleFree(search_path.xs);
    return EX_SUCCESS;
  }

  if (error) {
    PrintUsage(stderr);
    FbleFree(search_path.xs);
    return EX_USAGE_ERROR;
  }

  if (module_path == NULL) {
    fprintf(stderr, "missing required --module option.\n");
    PrintUsage(stderr);
    FbleFree(search_path.xs);
    return EX_USAGE_ERROR;
  }

  FbleValueHeap* heap = FbleNewValueHeap();
  FbleValue* linked = FbleLinkFromCompiledOrSource(heap, NULL, NULL, search_path, module_path);
  FbleFree(search_path.xs);
  if (linked == NULL) {
    FbleFreeValueHeap(heap);
    return EX_COMPILE_ERROR;
  }

  FbleValue* result = FbleEval(heap, linked, NULL);
  FbleReleaseValue(heap, linked);
  if (result == NULL) {
    FbleFreeValueHeap(heap);
    return EX_RUNTIME_ERROR;
  }

  printf("Result: ");
  for (size_t i = 0; i < 4; ++i) {
    FbleValue* bit = FbleStructValueAccess(result, i);
    printf("%c", FbleUnionValueTag(bit) == 0 ? '0' : '1');
  }
  printf("\n");

  FbleReleaseValue(heap, result);
  FbleFreeValueHeap(heap);
  return EX_SUCCESS;
}
