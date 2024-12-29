/**
 * @file main.c
 *  Implementation of fble main helper functions.
 */

#include <fble/fble-main.h>

#include <fble/fble-arg-parse.h>    // for FbleNewMainArg, etc.

// See documentation in fble-main.h
FbleMainArgs FbleNewMainArgs()
{
  FbleMainArgs args;
  args.module = FbleNewModuleArg();
  args.profile_file = NULL;
  args.deps_file = NULL;
  args.help = false;
  args.version = false;
  return args;
}

// See documentation in fble-main.h
void FbleFreeMainArgs(FbleMainArgs args)
{
  FbleFreeModuleArg(args.module);
}

// See documentation in fble-main.h
bool FbleParseMainArg(bool preloaded, FbleMainArgs* dest, int* argc, const char*** argv, bool* error)
{
  if (FbleParseBoolArg("-h", &dest->help, argc, argv, error)) return true;
  if (FbleParseBoolArg("--help", &dest->help, argc, argv, error)) return true;
  if (FbleParseBoolArg("-v", &dest->version, argc, argv, error)) return true;
  if (FbleParseBoolArg("--version", &dest->version, argc, argv, error)) return true;
  if (!preloaded && FbleParseModuleArg(&dest->module, argc, argv, error)) return true;
  if (FbleParseStringArg("--profile", &dest->profile_file, argc, argv, error)) return true;
  if (FbleParseStringArg("--deps-file", &dest->deps_file, argc, argv, error)) return true;
  if (FbleParseStringArg("--deps-target", &dest->deps_target, argc, argv, error)) return true;

  return false;
}
