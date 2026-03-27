/**
 * @file stdio.fble.c
 *  Implementation of /Core/Stdio/FFI%.
 */

#include "stdio.fble.h"

#include <locale.h>     // for setlocale, LC_CTYPE
#include <stdio.h>      // for FILE, fprintf, fflush, fgetc
#include <wchar.h>      // for wint_t, fgetwc

#include <fble/fble-alloc.h>  // for FbleFree
#include <fble/fble-value.h>  // for FbleValue, etc.

#include "char.fble.h"        // for FbleNewCharValue, FbleCharValueAccess
#include "int.fble.h"         // for FbleNewIntValue, FbleIntValueAccess
#include "string.fble.h"      // for FbleNewStringValue, FbleStringValueAccess

#define MAYBE_TAGWIDTH 1


/**
 * @func[GetStdin] FbleRunFunction to for GetStdin foreign function.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (Unit@, Unit@) { File@; }
 */  
static FbleValue* GetStdin(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)args;

  return FbleNewNativeValue(heap, stdin, NULL);
}

// /Core/Stdio/FFI%.GetStdin foreign function.
FbleForeignFunction _Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_GetStdin = {
  .path = "/Core/Stdio/FFI%",
  .name = "GetStdin",
  .num_args = 2,
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
 *   (Unit@, Unit@) { File@; }
 */  
static FbleValue* GetStdout(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)args;

  return FbleNewNativeValue(heap, stdout, NULL);
}

// /Core/Stdio/FFI%.GetStdout foreign function.
FbleForeignFunction _Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_GetStdout = {
  .path = "/Core/Stdio/FFI%",
  .name = "GetStdout",
  .num_args = 2,
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
 *   (Unit@, Unit@) { File@; }
 */  
static FbleValue* GetStderr(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)args;

  return FbleNewNativeValue(heap, stderr, NULL);
}

// /Core/Stdio/FFI%.GetStderr foreign function.
FbleForeignFunction _Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_GetStderr = {
  .path = "/Core/Stdio/FFI%",
  .name = "GetStderr",
  .num_args = 2,
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

  FbleValue* v = FbleNewNativeValue(heap, fout, NULL);
  return FbleNewUnionValue(heap, MAYBE_TAGWIDTH, 0, v);
}
// /Core/Stdio/FFI%.Open foreign function.
FbleForeignFunction _Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_Open = {
  .path = "/Core/Stdio/FFI%",
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
// /Core/Stdio/FFI%.Close foreign function.
FbleForeignFunction _Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_Close = {
  .path = "/Core/Stdio/FFI%",
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
 *   (File@, Unit@) { Maybe@<Char@>; }.
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

// /Core/Stdio/FFI%.GetChar foreign function.
FbleForeignFunction _Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_GetChar = {
  .path = "/Core/Stdio/FFI%",
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

// /Core/Stdio/FFI%.GetByte foreign function.
FbleForeignFunction _Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_GetByte = {
  .path = "/Core/Stdio/FFI%",
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

// /Core/Stdio/FFI%.PutChar foreign function.
FbleForeignFunction _Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_PutChar = {
  .path = "/Core/Stdio/FFI%",
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

// /Core/Stdio/FFI%.PutByte foreign function.
FbleForeignFunction _Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_PutByte = {
  .path = "/Core/Stdio/FFI%",
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

// /Core/Stdio/FFI%.Flush foreign function.
FbleForeignFunction _Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_Flush = {
  .path = "/Core/Stdio/FFI%",
  .name = "Flush",
  .num_args = 2,
  .max_call_args = 0,
  .run = &Flush,
};

// See documentation in stdio.fble.h
void FbleRegisterStdioForeignFunctions(FbleValueHeap* heap)
{
  setlocale(LC_CTYPE, "");

  FbleRegisterForeignFunction(heap, &_Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_GetStdin);
  FbleRegisterForeignFunction(heap, &_Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_GetStdout);
  FbleRegisterForeignFunction(heap, &_Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_GetStderr);
  FbleRegisterForeignFunction(heap, &_Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_Open);
  FbleRegisterForeignFunction(heap, &_Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_Close);
  FbleRegisterForeignFunction(heap, &_Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_GetChar);
  FbleRegisterForeignFunction(heap, &_Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_GetByte);
  FbleRegisterForeignFunction(heap, &_Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_PutChar);
  FbleRegisterForeignFunction(heap, &_Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_PutByte);
  FbleRegisterForeignFunction(heap, &_Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_Flush);
}
