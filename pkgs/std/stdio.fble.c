/**
 * @file stdio.fble.c
 *  Implementation of /Std/Io/File/Binary% functions.
 */

#include "stdio.fble.h"

#include <stdio.h>      // for FILE, fprintf, fflush, fgetc

#ifdef _WIN32
#include <fcntl.h>      // for _O_BINARY
#include <io.h>         // for _setmode
#endif // _WIN32

#include <fble/fble-alloc.h>    // for FbleFree
#include <fble/fble-runtime.h>  // for FbleValue, etc.

#include "data.fble.h"        // for FbleIntValueAccess, etc.

/**
 * @func[CloseFileOnFree] on_free function for closing a file.
 *  @arg[void*][file] The FILE* to close.
 *  @sideeffects Closes the file.
 */
static void CloseFileOnFree(void* file)
{
  fclose((FILE*)file);
}

/**
 * @func[GetStdin] FbleRunFunction to for GetStdin foreign function.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (Unit@) { File@; }
 */  
static FbleValue* GetStdin(
    FbleRuntime* runtime, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)args;

#ifdef __WIN32
  _setmode(_fileno(stdin), _O_BINARY);
#endif // __WIN32

  return FbleNewNativeValue(runtime, stdin, NULL);
}

// /Std/Io/File/Binary%.GetStdin foreign function.
FbleForeign _Fble_2f_Std_2f_Io_2f_File_2f_Binary_25__2e_GetStdin = {
  .path = "/Std/Io/File/Binary%",
  .name = "GetStdin",
  .num_args = 1,
  .max_call_args = 0,
  .run = &GetStdin,
};

/**
 * @func[GetStdout] FbleRunFunction to for GetStdout foreign function.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (Unit@) { File@; }
 */  
static FbleValue* GetStdout(
    FbleRuntime* runtime, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)args;

#ifdef __WIN32
  _setmode(_fileno(stdout), _O_BINARY);
#endif // __WIN32

  return FbleNewNativeValue(runtime, stdout, NULL);
}

// /Std/Io/File/Binary%.GetStdout foreign function.
FbleForeign _Fble_2f_Std_2f_Io_2f_File_2f_Binary_25__2e_GetStdout = {
  .path = "/Std/Io/File/Binary%",
  .name = "GetStdout",
  .num_args = 1,
  .max_call_args = 0,
  .run = &GetStdout,
};

/**
 * @func[GetStderr] FbleRunFunction to for GetStderr foreign function.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (Unit@) { File@; }
 */  
static FbleValue* GetStderr(
    FbleRuntime* runtime, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)args;

#ifdef __WIN32
  _setmode(_fileno(stderr), _O_BINARY);
#endif// __WIN32

  return FbleNewNativeValue(runtime, stderr, NULL);
}

// /Std/Io/File/Binary%.GetStderr foreign function.
FbleForeign _Fble_2f_Std_2f_Io_2f_File_2f_Binary_25__2e_GetStderr = {
  .path = "/Std/Io/File/Binary%",
  .name = "GetStderr",
  .num_args = 1,
  .max_call_args = 0,
  .run = &GetStderr,
};

/**
 * @func[Open] FbleRunFunction to for Open foreign function.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (String@, String@, Unit@) { Maybe@<File@>; }
 */  
static FbleValue* Open(
    FbleRuntime* runtime, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)args;

  char* file = FbleStringValueAccess(args[0]);
  char* mode = FbleStringValueAccess(args[1]);
  FILE* fout = fopen(file, mode);
  FbleFree(file);
  FbleFree(mode);

  if (fout == NULL) {
    return FbleNewMaybeValue(runtime, NULL);
  }

  FbleValue* v = FbleNewNativeValue(runtime, fout, &CloseFileOnFree);
  return FbleNewMaybeValue(runtime, v);
}
// /Std/Io/File/Binary%.Open foreign function.
FbleForeign _Fble_2f_Std_2f_Io_2f_File_2f_Binary_25__2e_Open = {
  .path = "/Std/Io/File/Binary%",
  .name = "Open",
  .num_args = 3,
  .max_call_args = 0,
  .run = &Open,
};

/**
 * @func[GetByte] FbleRunFunction for GetByte foreign function.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (File@, Unit@) { Maybe@<Int@>; }.
 *
 *  @sideeffects
 *   Reads a byte from the give file.
 */
static FbleValue* GetByte(
    FbleRuntime* runtime, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)args;

  FILE* file = (FILE*)FbleNativeValueData(args[0]);

  int c = fgetc(file);
  if (c == EOF) {
    return FbleNewMaybeValue(runtime, NULL);
  }

  FbleValue* v = FbleNewIntValue(runtime, c);
  return FbleNewMaybeValue(runtime, v);
}

// /Std/Io/File/Binary%.GetByte foreign function.
FbleForeign _Fble_2f_Std_2f_Io_2f_File_2f_Binary_25__2e_GetByte = {
  .path = "/Std/Io/File/Binary%",
  .name = "GetByte",
  .num_args = 2,
  .max_call_args = 0,
  .run = &GetByte,
};

/**
 * @func[PutByte] FbleRunFunction for PutByte foreign function.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (File@, Int@, Unit@) { Unit@; }.
 *
 *  @sideeffects
 *   Writes a byte to the give file.
 */
static FbleValue* PutByte(
    FbleRuntime* runtime, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)args;

  FILE* file = (FILE*)FbleNativeValueData(args[0]);
  int byte = FbleIntValueAccess(args[1]);
  fputc(byte, file);
  return FbleNewStructValue_(runtime, 0);
}

// /Std/Io/File/Binary%.PutByte foreign function.
FbleForeign _Fble_2f_Std_2f_Io_2f_File_2f_Binary_25__2e_PutByte = {
  .path = "/Std/Io/File/Binary%",
  .name = "PutByte",
  .num_args = 3,
  .max_call_args = 0,
  .run = &PutByte,
};

/**
 * @func[Flush] FbleRunFunction for Flush foreign function.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (File@, Unit@) { Unit@; }.
 *
 *  @sideeffects
 *   Flushes the given file.
 */
static FbleValue* Flush(
    FbleRuntime* runtime, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)args;

  FILE* file = (FILE*)FbleNativeValueData(args[0]);
  fflush(file);
  return FbleNewStructValue_(runtime, 0);
}

// /Std/Io/File/Binary%.Flush foreign function.
FbleForeign _Fble_2f_Std_2f_Io_2f_File_2f_Binary_25__2e_Flush = {
  .path = "/Std/Io/File/Binary%",
  .name = "Flush",
  .num_args = 2,
  .max_call_args = 0,
  .run = &Flush,
};

// See documentation in stdio.fble.h
void FbleRegisterStdioForeignValues(FbleRuntime* runtime)
{
  FbleRegisterForeignValue(runtime, &_Fble_2f_Std_2f_Io_2f_File_2f_Binary_25__2e_GetStdin);
  FbleRegisterForeignValue(runtime, &_Fble_2f_Std_2f_Io_2f_File_2f_Binary_25__2e_GetStdout);
  FbleRegisterForeignValue(runtime, &_Fble_2f_Std_2f_Io_2f_File_2f_Binary_25__2e_GetStderr);
  FbleRegisterForeignValue(runtime, &_Fble_2f_Std_2f_Io_2f_File_2f_Binary_25__2e_Open);
  FbleRegisterForeignValue(runtime, &_Fble_2f_Std_2f_Io_2f_File_2f_Binary_25__2e_PutByte);
  FbleRegisterForeignValue(runtime, &_Fble_2f_Std_2f_Io_2f_File_2f_Binary_25__2e_GetByte);
  FbleRegisterForeignValue(runtime, &_Fble_2f_Std_2f_Io_2f_File_2f_Binary_25__2e_Flush);
}
