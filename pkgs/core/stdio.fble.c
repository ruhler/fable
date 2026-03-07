/**
 * @file stdio.fble.c
 *  Implementation of /Core/Stdio/Native%.
 */

#include "stdio.fble.h"

#include <locale.h>     // for setlocale, LC_CTYPE
#include <stdlib.h>     // for getenv
#include <stdio.h>      // for FILE, fprintf, fflush, fgetc
#include <wchar.h>      // for wint_t, fgetwc

#include <fble/fble-alloc.h>       // for FbleFree
#include <fble/fble-program.h>     // for FblePreloadedModule
#include <fble/fble-value.h>       // for FbleValue, etc.

#include "char.fble.h"        // for FbleNewCharValue, FbleCharValueAccess
#include "int.fble.h"         // for FbleNewIntValue, FbleIntValueAccess
#include "string.fble.h"      // for FbleNewStringValue, FbleStringValueAccess

#define MAYBE_TAGWIDTH 1
#define BOOL_TAGWIDTH 1
#define LIST_TAGWIDTH 1
#define RESULT_FIELDC 2
#define MODE_TAGWIDTH 1

static FbleValue* IStreamImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);
static FbleValue* OStreamImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);
static FbleValue* ReadImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);
static FbleValue* WriteImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);
static FbleValue* StdioImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);
static FbleValue* IStream(FbleValueHeap* heap, FILE* file, FbleBlockId profile_block_id);
static FbleValue* OStream(FbleValueHeap* heap, FILE* file, FbleBlockId profile_block_id);
static FbleValue* Read(FbleValueHeap* heap, FbleBlockId profile_block_id);
static FbleValue* Write(FbleValueHeap* heap, FbleBlockId profile_block_id);
static FbleValue* Stdio(FbleValueHeap* heap, FbleBlockId profile_block_id);


// -Wpedantic doesn't like our initialization of flexible array members when
// defining static FbleString values.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

static FbleString Filename = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = __FILE__, };
static FbleString StrCore = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "Core", };
static FbleString StrStdio = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "Stdio", };
static FbleString StrNative = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "Native", };

static FbleString StrModuleBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Core/Stdio/Native%", };
static FbleString StrICharStreamBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Core/Stdio/Native%.ICharStream", };
static FbleString StrIStreamBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Core/Stdio/Native%.IStream", };
static FbleString StrOStreamBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Core/Stdio/Native%.OStream", };
static FbleString StrReadBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Core/Stdio/Native%.Read", };
static FbleString StrWriteBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Core/Stdio/Native%.Write", };
static FbleString StrStdioBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Core/Stdio/Native%.Stdio", };
static FbleString StrStdinBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Core/Stdio/Native%.Stdin", };

#pragma GCC diagnostic pop

#define NUM_PROFILE_BLOCKS 8
static FbleName ProfileBlocks[] = {
  { .name = &StrModuleBlock, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrICharStreamBlock, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrIStreamBlock, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrOStreamBlock, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrReadBlock, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrWriteBlock, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrStdioBlock, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrStdinBlock, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
};

/**
 * @func[IStreamImpl] FbleRunFunction to read a byte from a file.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is @l{(Unit@) { @<Maybe@<Int@>>; }}.
 *
 *  @sideeffects
 *   Reads a byte from the file.
 */
static FbleValue* IStreamImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)args;

  FILE* file = (FILE*)FbleNativeValueData(function->statics[0]);

  int c = fgetc(file);
  if (c == EOF) {
    return FbleNewEnumValue(heap, MAYBE_TAGWIDTH, 1);
  }

  FbleValue* v = FbleNewIntValue(heap, c);
  return FbleNewUnionValue(heap, MAYBE_TAGWIDTH, 0, v);
}

/**
 * @func[OStreamImpl] FbleRunFunction to write a byte to a file.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is @l{(Int@, Unit@) { Unit@; }}.
 */
static FbleValue* OStreamImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;

  FILE* file = (FILE*)FbleNativeValueData(function->statics[0]);

  FbleValue* byte = args[0];

  int64_t c = FbleIntValueAccess(byte);
  if (c == -1) {
    fclose(file);
  } else {
    fputc(c, file);
    fflush(file);
  }

  return FbleNewStructValue_(heap, 0);
}

/**
 * @func[ReadImpl] FbleRunFunction to open a file for reading.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (String@, Mode@, Unit@) { Maybe@<IStream@<R@>>; }
 */
static FbleValue* ReadImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;

  char* filename = FbleStringValueAccess(args[0]);
  const char* mode = FbleUnionValueTag(args[1], MODE_TAGWIDTH) ? "rb" : "r";

  FILE* fin = fopen(filename, mode);
  FbleFree(filename);

  if (fin == NULL) {
    return FbleNewEnumValue(heap, MAYBE_TAGWIDTH, 1); // Nothing
  }

  // The Read block id is index 4 from the main block id
  FbleBlockId module_block_id = function->profile_block_id - 4;
  FbleValue* stream = IStream(heap, fin, module_block_id);
  return FbleNewUnionValue(heap, MAYBE_TAGWIDTH, 0, stream); // Just(stream)
}

/**
 * @func[WriteImpl] FbleRunFunction to open a file for writing.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (String@, Mode@, Unit@) { Maybe@<OStream@<R@>>; }
 */
static FbleValue* WriteImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;

  char* filename = FbleStringValueAccess(args[0]);
  const char* mode = FbleUnionValueTag(args[1], MODE_TAGWIDTH) ? "wb" : "w";

  FILE* fin = fopen(filename, mode);
  FbleFree(filename);

  if (fin == NULL) {
    return FbleNewEnumValue(heap, MAYBE_TAGWIDTH, 1); // Nothing
  }

  // The Write block id is index 5 from the main block id
  FbleBlockId module_block_id = function->profile_block_id - 5;
  FbleValue* stream = OStream(heap, fin, module_block_id);
  return FbleNewUnionValue(heap, MAYBE_TAGWIDTH, 0, stream); // Just(stream)
}

/**
 * @func[StdioImpl] FbleRunFunction to generate the native Stdio@ interface.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (Native@<M@>, Monad@<M@>, Unit@) { Stdio@<M@>; }
 */  
static FbleValue* StdioImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)args;

  // The Stdio block id is index 6 from the main block id
  FbleBlockId module_block_id = function->profile_block_id - 6;

  FblePushFrame(heap);
  FbleValue* fble_stdin = IStream(heap, stdin, module_block_id);
  FbleValue* fble_stdout = OStream(heap, stdout, module_block_id);
  FbleValue* fble_stderr = OStream(heap, stderr, module_block_id);
  FbleValue* fble_read = Read(heap, module_block_id);
  FbleValue* fble_write = Write(heap, module_block_id);
  FbleValue* fble_stdio = FbleNewStructValue_(heap, 5,
      fble_stdin, fble_stdout, fble_stderr,
      fble_read, fble_write);
  return FblePopFrame(heap, fble_stdio);
}

/**
 * @func[IStream] Allocates an @l{IStream@} for a file.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FILE*][file] The FILE to allocate the @l{IStream@} for.
 *  @arg[FbleBlockId][module_block_id]
 *   The block_id of the /Core/Stdio/Native% block.
 *
 *  @returns[FbleValue*] An fble @l{IStream@} function value.
 *
 *  @sideeffects
 *   Allocates a value on the heap.
 */
static FbleValue* IStream(FbleValueHeap* heap, FILE* file, FbleBlockId module_block_id)
{
  FbleValue* native = FbleNewNativeValue(heap, file, NULL);

  FbleExecutable exe = {
    .num_args = 1,
    .num_statics = 1,
    .max_call_args = 0,
    .run = &IStreamImpl,
  };

  return FbleNewFuncValue(heap, &exe, module_block_id + 2, &native);
}

/**
 * @func[OStream] Allocates an @l{OStream@} for a file.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FILE*][file] The FILE to allocate the @l{OStream@} for.
 *  @arg[FbleBlockId][module_block_id]
 *   The block_id of the /Core/Stdio/Native% block.
 *
 *  @returns[FbleValue*] An fble @l{OStream@} function value.
 *
 *  @sideeffects
 *   Allocates a value on the heap.
 */
static FbleValue* OStream(FbleValueHeap* heap, FILE* file, FbleBlockId module_block_id)
{
  FbleValue* native = FbleNewNativeValue(heap, file, NULL);

  FbleExecutable exe = {
    .num_args = 2,
    .num_statics = 1,
    .max_call_args = 0,
    .run = &OStreamImpl,
  };

  return FbleNewFuncValue(heap, &exe, module_block_id + 3, &native);
}

/**
 * @func[Read] Allocates an fble read function.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleBlockId][module_block_id]
 *   The block_id of the /Core/Stdio/Native% block.
 *  @returns[FbleValue*] The allocated function.
 *  @sideeffects Allocates an fble value.
 */
static FbleValue* Read(FbleValueHeap* heap, FbleBlockId module_block_id)
{
  FbleExecutable exe = {
    .num_args = 3,
    .num_statics = 0,
    .max_call_args = 0,
    .run = &ReadImpl,
  };
  return FbleNewFuncValue(heap, &exe, module_block_id + 4, NULL);
}

/**
 * @func[Write] Allocates an fble write function.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleBlockId][module_block_id]
 *   The block_id of the /Core/Stdio/Native% block.
 *  @returns[FbleValue*] The allocated function.
 *  @sideeffects Allocates an fble value.
 */
static FbleValue* Write(FbleValueHeap* heap, FbleBlockId module_block_id)
{
  FbleExecutable exe = {
    .num_args = 3,
    .num_statics = 0,
    .max_call_args = 0,
    .run = &WriteImpl,
  };
  return FbleNewFuncValue(heap, &exe, module_block_id + 5, NULL);
}

/**
 * @func[Stdio] Allocates an fble Stdio function.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleBlockId][module_block_id]
 *   The block_id of the /Core/Stdio/Native% block.
 *  @returns[FbleValue*] The allocated function.
 *  @sideeffects Allocates an fble value.
 */
static FbleValue* Stdio(FbleValueHeap* heap, FbleBlockId module_block_id) {
  FbleExecutable exe = {
    .num_args = 3,
    .num_statics = 0,
    .max_call_args = 0,
    .run = &StdioImpl,
  };
  return FbleNewFuncValue(heap, &exe, module_block_id + 6, NULL);
}

static FbleName Core_Stdio_Native_PathEntries[] = {
  { .name = &StrCore, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrStdio, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
  { .name = &StrNative, .space = 0, .loc = { .source = &Filename, .line = __LINE__, .col = 1 }},
};

static FbleModulePath Core_Stdio_Native_Path = {
  .refcount = 1,
  .magic = FBLE_MODULE_PATH_MAGIC,
  .loc = { .source = &Filename, .line = __LINE__, .col = 1 },
  .path = { .size = 3, .xs = Core_Stdio_Native_PathEntries},
};

static FbleValue* Core_Stdio_Native_Run(FbleValueHeap* heap, FbleProfileThread* profile, FbleFunction* function, FbleValue** args)
{
  setlocale(LC_CTYPE, "");

  FbleBlockId block_id = function->profile_block_id;
  FblePushFrame(heap);
  FbleValue* fble_stdio = Stdio(heap, block_id);
  FbleValue* fble_module = FbleNewStructValue_(heap, 1, fble_stdio);
  return FblePopFrame(heap, fble_module);
}

static FbleExecutable Core_Stdio_Native_Executable = {
  .num_args = 0, 
  .num_statics = 0,
  .max_call_args = 0,
  .run = &Core_Stdio_Native_Run,
};

FblePreloadedModule _Fble_2f_Core_2f_Stdio_2f_Native_25_ = {
  .path = &Core_Stdio_Native_Path,
  .deps = { .size = 0, .xs = NULL },
  .executable = &Core_Stdio_Native_Executable,
  .profile_blocks = { .size = NUM_PROFILE_BLOCKS, .xs = ProfileBlocks },
};

/**
 * @func[GetStdin] FbleRunFunction to for GetStdin foreign function.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (Native@<M@>, Monad@<M@>, Unit@) { File@; }
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
  .num_args = 3,
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
 *   (Native@<M@>, Monad@<M@>, Unit@) { File@; }
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
  .num_args = 3,
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
 *   (Native@<M@>, Monad@<M@>, Unit@) { File@; }
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
  .num_args = 3,
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
 *   (Native@<M@>, Monad@<M@>, String@, String@, Unit@) { Maybe@<File@>; }
 */  
static FbleValue* Open(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)args;

  char* file = FbleStringValueAccess(args[2]);
  char* mode = FbleStringValueAccess(args[3]);
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
  .num_args = 5,
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
 *   (Native@<M@>, Monad@<M@>, File@, Unit@) { Unit@; }
 */  
static FbleValue* Close(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)args;

  FILE* file = (FILE*)FbleNativeValueData(args[2]);
  fclose(file);
  return FbleNewStructValue_(heap, 0);
}
// /Core/Stdio/FFI%.Close foreign function.
FbleForeignFunction _Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_Close = {
  .path = "/Core/Stdio/FFI%",
  .name = "Close",
  .num_args = 4,
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
 *   (Native@<M@>, Monad@<M@>, File@, Unit@) { Maybe@<Char@>; }.
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

  FILE* file = (FILE*)FbleNativeValueData(args[2]);

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
  .num_args = 4,
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
 *   (Native@<M@>, Monad@<M@>, File@, Unit@) { Maybe@<Int@>; }.
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

  FILE* file = (FILE*)FbleNativeValueData(args[2]);

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
  .num_args = 4,
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
 *   (Native@<M@>, Monad@<M@>, File@, Char@, Unit@) { Unit@; }.
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

  FILE* file = (FILE*)FbleNativeValueData(args[2]);
  wchar_t c = FbleCharValueAccess(args[3]);
  fputwc(c, file);
  return FbleNewStructValue_(heap, 0);
}

// /Core/Stdio/FFI%.PutChar foreign function.
FbleForeignFunction _Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_PutChar = {
  .path = "/Core/Stdio/FFI%",
  .name = "PutChar",
  .num_args = 5,
  .max_call_args = 0,
  .run = &PutChar,
};

/**
 * @func[Flush] FbleRunFunction for Flush foreign function.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (Native@<M@>, Monad@<M@>, File@, Unit@) { Unit@; }.
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

  FILE* file = (FILE*)FbleNativeValueData(args[2]);
  fflush(file);
  return FbleNewStructValue_(heap, 0);
}

// /Core/Stdio/FFI%.Flush foreign function.
FbleForeignFunction _Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_Flush = {
  .path = "/Core/Stdio/FFI%",
  .name = "Flush",
  .num_args = 4,
  .max_call_args = 0,
  .run = &Flush,
};

// See documentation in stdio.fble.h
void FbleRegisterStdioForeignFunctions(FbleValueHeap* heap)
{
  FbleRegisterForeignFunction(heap, &_Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_GetStdin);
  FbleRegisterForeignFunction(heap, &_Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_GetStdout);
  FbleRegisterForeignFunction(heap, &_Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_GetStderr);
  FbleRegisterForeignFunction(heap, &_Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_Open);
  FbleRegisterForeignFunction(heap, &_Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_Close);
  FbleRegisterForeignFunction(heap, &_Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_GetChar);
  FbleRegisterForeignFunction(heap, &_Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_GetByte);
  FbleRegisterForeignFunction(heap, &_Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_PutChar);
  FbleRegisterForeignFunction(heap, &_Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_Flush);
}
