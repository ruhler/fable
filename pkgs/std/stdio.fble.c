/**
 * @file stdio.fble.c
 *  Implementation of /Std/Io/File/Internal% functions.
 */

#include "stdio.fble.h"

#include <locale.h>     // for setlocale, LC_CTYPE
#include <stdio.h>      // for FILE, fprintf, fflush, fgetc
#include <wchar.h>      // for wint_t, fgetwc

#include <fble/fble-alloc.h>  // for FbleFree
#include <fble/fble-value.h>  // for FbleValue, etc.

#include "data.fble.h"        // for FbleNewCharValue, etc.

#define MAYBE_TAGWIDTH 1

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
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)args;

  return FbleNewNativeValue(heap, stdin, NULL);
}

// /Std/Io/File/Internal%.GetStdin foreign function.
FbleForeign _Fble_2f_Std_2f_Io_2f_File_2f_Internal_25__2e_GetStdin = {
  .path = "/Std/Io/File/Internal%",
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
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)args;

  return FbleNewNativeValue(heap, stdout, NULL);
}

// /Std/Io/File/Internal%.GetStdout foreign function.
FbleForeign _Fble_2f_Std_2f_Io_2f_File_2f_Internal_25__2e_GetStdout = {
  .path = "/Std/Io/File/Internal%",
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
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)args;

  return FbleNewNativeValue(heap, stderr, NULL);
}

// /Std/Io/File/Internal%.GetStderr foreign function.
FbleForeign _Fble_2f_Std_2f_Io_2f_File_2f_Internal_25__2e_GetStderr = {
  .path = "/Std/Io/File/Internal%",
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
    FbleValueHeap* heap, FbleProfileThread* profile,
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
    return FbleNewEnumValue(heap, MAYBE_TAGWIDTH, 1);
  }

  FbleValue* v = FbleNewNativeValue(heap, fout, &CloseFileOnFree);
  return FbleNewUnionValue(heap, MAYBE_TAGWIDTH, 0, v);
}
// /Std/Io/File/Internal%.Open foreign function.
FbleForeign _Fble_2f_Std_2f_Io_2f_File_2f_Internal_25__2e_Open = {
  .path = "/Std/Io/File/Internal%",
  .name = "Open",
  .num_args = 3,
  .max_call_args = 0,
  .run = &Open,
};

/**
 * @func[Close] FbleRunFunction to for Close foreign function.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (File@, Unit@) { Unit@; }
 */  
static FbleValue* Close(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)args;

  FILE* file = (FILE*)FbleNativeValueData(args[0]);
  fclose(file);
  return FbleNewStructValue_(heap, 0);
}
// /Std/Io/File/Internal%.Close foreign function.
FbleForeign _Fble_2f_Std_2f_Io_2f_File_2f_Internal_25__2e_Close = {
  .path = "/Std/Io/File/Internal%",
  .name = "Close",
  .num_args = 2,
  .max_call_args = 0,
  .run = &Close,
};

/**
 * @func[GetChar] FbleRunFunction for GetChar foreign function.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (File@, Unit@) { Maybe@<Char@>; }
 *
 *  @sideeffects
 *   Reads a character from the give file.
 */
static FbleValue* GetChar(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)args;

  FILE* file = (FILE*)FbleNativeValueData(args[0]);

  wint_t c = fgetwc(file);
  if (c == WEOF) {
    return FbleNewEnumValue(heap, MAYBE_TAGWIDTH, 1);
  }

  FbleValue* v = FbleNewCharValue(heap, c);
  return FbleNewUnionValue(heap, MAYBE_TAGWIDTH, 0, v);
}

// /Std/Io/File/Internal%.GetChar foreign function.
FbleForeign _Fble_2f_Std_2f_Io_2f_File_2f_Internal_25__2e_GetChar = {
  .path = "/Std/Io/File/Internal%",
  .name = "GetChar",
  .num_args = 2,
  .max_call_args = 0,
  .run = &GetChar,
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
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)args;

  FILE* file = (FILE*)FbleNativeValueData(args[0]);

  int c = fgetc(file);
  if (c == EOF) {
    return FbleNewEnumValue(heap, MAYBE_TAGWIDTH, 1);
  }

  FbleValue* v = FbleNewIntValue(heap, c);
  return FbleNewUnionValue(heap, MAYBE_TAGWIDTH, 0, v);
}

// /Std/Io/File/Internal%.GetByte foreign function.
FbleForeign _Fble_2f_Std_2f_Io_2f_File_2f_Internal_25__2e_GetByte = {
  .path = "/Std/Io/File/Internal%",
  .name = "GetByte",
  .num_args = 2,
  .max_call_args = 0,
  .run = &GetByte,
};

/**
 * @func[PutChar] FbleRunFunction for PutChar foreign function.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (File@, Char@, Unit@) { Unit@; }.
 *
 *  @sideeffects
 *   Writes a character to the give file.
 */
static FbleValue* PutChar(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)args;

  FILE* file = (FILE*)FbleNativeValueData(args[0]);
  wchar_t c = FbleCharValueAccess(args[1]);
  fputwc(c, file);
  return FbleNewStructValue_(heap, 0);
}

// /Std/Io/File/Internal%.PutChar foreign function.
FbleForeign _Fble_2f_Std_2f_Io_2f_File_2f_Internal_25__2e_PutChar = {
  .path = "/Std/Io/File/Internal%",
  .name = "PutChar",
  .num_args = 3,
  .max_call_args = 0,
  .run = &PutChar,
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
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)args;

  FILE* file = (FILE*)FbleNativeValueData(args[0]);
  int byte = FbleIntValueAccess(args[1]);
  fputc(byte, file);
  return FbleNewStructValue_(heap, 0);
}

// /Std/Io/File/Internal%.PutByte foreign function.
FbleForeign _Fble_2f_Std_2f_Io_2f_File_2f_Internal_25__2e_PutByte = {
  .path = "/Std/Io/File/Internal%",
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
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)args;

  FILE* file = (FILE*)FbleNativeValueData(args[0]);
  fflush(file);
  return FbleNewStructValue_(heap, 0);
}

// /Std/Io/File/Internal%.Flush foreign function.
FbleForeign _Fble_2f_Std_2f_Io_2f_File_2f_Internal_25__2e_Flush = {
  .path = "/Std/Io/File/Internal%",
  .name = "Flush",
  .num_args = 2,
  .max_call_args = 0,
  .run = &Flush,
};

// See documentation in stdio.fble.h
void FbleRegisterStdioForeignValues(FbleValueHeap* heap)
{
  setlocale(LC_CTYPE, "");

  FbleRegisterForeignValue(heap, &_Fble_2f_Std_2f_Io_2f_File_2f_Internal_25__2e_GetStdin);
  FbleRegisterForeignValue(heap, &_Fble_2f_Std_2f_Io_2f_File_2f_Internal_25__2e_GetStdout);
  FbleRegisterForeignValue(heap, &_Fble_2f_Std_2f_Io_2f_File_2f_Internal_25__2e_GetStderr);
  FbleRegisterForeignValue(heap, &_Fble_2f_Std_2f_Io_2f_File_2f_Internal_25__2e_Open);
  FbleRegisterForeignValue(heap, &_Fble_2f_Std_2f_Io_2f_File_2f_Internal_25__2e_Close);
  FbleRegisterForeignValue(heap, &_Fble_2f_Std_2f_Io_2f_File_2f_Internal_25__2e_GetChar);
  FbleRegisterForeignValue(heap, &_Fble_2f_Std_2f_Io_2f_File_2f_Internal_25__2e_GetByte);
  FbleRegisterForeignValue(heap, &_Fble_2f_Std_2f_Io_2f_File_2f_Internal_25__2e_PutChar);
  FbleRegisterForeignValue(heap, &_Fble_2f_Std_2f_Io_2f_File_2f_Internal_25__2e_PutByte);
  FbleRegisterForeignValue(heap, &_Fble_2f_Std_2f_Io_2f_File_2f_Internal_25__2e_Flush);
}
