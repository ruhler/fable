// io.c --
//
//   This file implements routines for reading and writing fblc values and
//   programs to and from strings and files.

#include <assert.h>
#include <stdint.h>   // for uint32_t
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "fblc.h"

// BitSource --
//   Structure representing a source of bits for reading a program or value.
//   Bits are formated as a sequence of ascii binary digits '0' and '1'. A
//   BitSource is either a character string or an open file.
//
// Fields:
//   string - String of bits or NULL if the source of bits is not a string.
//   fd - File descriptor of open file for reading bits, or -1 if the source
//        of bits is not a file.
//   synced - Whether any bits have been read from the source.
typedef struct {
  const char* string;
  int fd;
  bool synced;
} BitSource;

static int ReadBit(BitSource* source);
static uint32_t ReadBits(BitSource* source, size_t num_bits);
static void SyncBitSource(BitSource* source);

// BitSink --
//   Structure representing a sink for writing bits.
//   Bits are formated as a sequence of ascii binary digits '0' and '1'.
//
// Fields:
//   fd - File descriptor of an open file for writing bits.
//   synced - Whether any bits have been written to the sink.
typedef struct {
  int fd;
  bool synced;
} BitSink;

static void WriteBits(BitSink* sink, size_t num_bits, uint32_t bits);
static void SyncBitSink(BitSink* sink);

static size_t ReadId(BitSource* source);
static FblcExpr* ReadExpr(FblcArena* arena, BitSource* source);
static FblcActn* ReadActn(FblcArena* arena, BitSource* source);
static FblcDecl* ReadDecl(FblcArena* arena, BitSource* source);
static FblcProgram* ReadProgram(FblcArena* arena, BitSource* source);

static size_t TagSize(size_t fieldc);
static FblcValue* ReadValue(FblcArena* arena, BitSource* source, FblcProgram* prg, FblcTypeId type);
static void WriteValue(BitSink* bits, FblcValue* value);

// ReadBit --
//   Read the next bit from the given bit source.
//
// Inputs:
//   source - the bit source to read from.
//
// Returns:
//   The next bit in the stream. EOF if the end of the stream has been
//   reached.
//
// Side effects:
//   Advance the stream by a single bit.
static int ReadBit(BitSource* source)
{
  source->synced = true;
  int bit = EOF;
  if (source->string != NULL && *source->string != '\0') {
    bit = *source->string;
    source->string++;
  } else if (source->fd >= 0) {
    char c;
    if(read(source->fd, &c, 1)) {
      bit = c;
    }
  }

  switch (bit) {
    case '0': return 0;
    case '1': return 1;
    case EOF: return EOF;

    default:
      fprintf(stderr, "Unexpected char in bit source: '%c'", bit);
      assert(false);
      return EOF;
  }
}

// ReadBits --
//   Read num_bits bits from the given bit source.
//
// Inputs:
//   source - the bit source to read from.
//   num_bits - the number of bits to read.
//
// Returns:
//   The next num_bits bits in the stream, zero extended. Returns EOF on error
//   or if there are insufficient bits remaining in the file to satisfy the
//   request.
//
// Side effects:
//   Advance the stream by num_bits bits.
//   The behavior is undefined if num_bits is greater than 31.
static uint32_t ReadBits(BitSource* source, size_t num_bits)
{
  assert(num_bits < 32 && "ReadBits invalid argument");
  uint32_t bits = 0;
  for (size_t i = 0; i < num_bits; ++i) {
    int bit = ReadBit(source);
    if (bit == EOF) {
      return EOF;
    }
    bits = (bits << 1) | bit;
  }
  return bits;
}

// SyncBitSource --
//   Ensure at least one bit has been read from the given bit source.
//
// Inputs:
//   source - The bit source to sync.
//
// Results:
//   None.
//
// Side effects:
//   If no bit has been read from the bit source so far, reads a bit from the
//   bit source. The bit read is ignored.
static void SyncBitSource(BitSource* source)
{
  if (!source->synced) {
    ReadBit(source);
  }
}

// WriteBits --
//   Write num_bits bits to the given bit sink.
//
// Inputs:
//   sink - the bit sink to write to.
//   num_bits - the number of bits to write. This must be less than 32.
//   bits - the actual bits to write.
//
// Returns:
//   None.
//
// Side effects:
//   num_bits bits are written to the stream in the form of binary ascii
//   digits '0' and '1'.
static void WriteBits(BitSink* sink, size_t num_bits, uint32_t bits)
{
  assert(num_bits < 32 && "WriteBits invalid num_bits");
  for (uint32_t mask = ((1 << num_bits) >> 1); mask > 0; mask >>= 1) {
    char c = (bits & mask) ? '1' : '0';
    write(sink->fd, &c, 1);
    sink->synced = true;
  }
}

// SyncBitSink --
//   Ensure at least one bit has been written to the given bit sink.
//
// Inputs:
//   sink - the bit sink to sync.
//
// Returns:
//   None.
//
// Side effects:
//   If no bit has been written from the bit sink so far, writes a bit to the
//   bit sink.
static void SyncBitSink(BitSink* sink)
{
  if (!sink->synced) {
    WriteBits(sink, 1, 0);
  }
}

// ReadId --
//   Read an id from the given bit source.
//
// Inputs:
//   source - The source of bits to read from.
//
// Returns:
//   The id read from the source.
//
// Side effects:
//   Reads bits from the bit source.
static size_t ReadId(BitSource* source)
{
  switch (ReadBits(source, 2)) {
    case 0: return 0;
    case 1: return 1;
    case 2: return 2 * ReadId(source);
    case 3: return 2 * ReadId(source) + 1;
    default: assert(false && "Unsupported Id tag");
  }
}

// ReadExpr --
//   Read an expression from a bit source.
//
// Inputs:
//   arena - allocator to use to build the expression.
//   source - The bit source to read the expression from.
//
// Results:
//   The expression read from the bit source.
//
// Side effects:
//   Bits are aread from the bit source.
static FblcExpr* ReadExpr(FblcArena* arena, BitSource* source)
{
  switch (ReadBits(source, 3)) {
    case FBLC_VAR_EXPR: {
      FblcVarExpr* expr = arena->alloc(arena, sizeof(FblcVarExpr));
      expr->tag = FBLC_VAR_EXPR;
      expr->var = ReadId(source);
      return (FblcExpr*)expr;
    }

    case FBLC_APP_EXPR: {
      FblcAppExpr* expr = arena->alloc(arena, sizeof(FblcAppExpr));
      expr->tag = FBLC_APP_EXPR;
      expr->func = ReadId(source);
      FblcVectorInit(arena, expr->argv, expr->argc);
      while (ReadBits(source, 1)) {
        FblcVectorAppend(arena, expr->argv, expr->argc, ReadExpr(arena, source));
      }
      return (FblcExpr*)expr;
    }

    case FBLC_UNION_EXPR: {
      FblcUnionExpr* expr = arena->alloc(arena, sizeof(FblcUnionExpr));
      expr->tag = FBLC_UNION_EXPR;
      expr->type = ReadId(source);
      expr->field = ReadId(source);
      expr->body = ReadExpr(arena, source);
      return (FblcExpr*)expr;
    }

    case FBLC_ACCESS_EXPR: {
      FblcAccessExpr* expr = arena->alloc(arena, sizeof(FblcAccessExpr));
      expr->tag = FBLC_ACCESS_EXPR;
      expr->object = ReadExpr(arena, source);
      expr->field = ReadId(source);
      return (FblcExpr*)expr;
    }

    case FBLC_COND_EXPR: {
      FblcCondExpr* expr = arena->alloc(arena, sizeof(FblcCondExpr));
      expr->tag = FBLC_COND_EXPR;
      expr->select = ReadExpr(arena, source);
      FblcVectorInit(arena, expr->argv, expr->argc);
      do { 
        FblcVectorAppend(arena, expr->argv, expr->argc, ReadExpr(arena, source));
      } while (ReadBits(source, 1));
      return (FblcExpr*)expr;
    }

    case FBLC_LET_EXPR: {
      FblcLetExpr* expr = arena->alloc(arena, sizeof(FblcLetExpr));
      expr->tag = FBLC_LET_EXPR;
      expr->def = ReadExpr(arena, source);
      expr->body = ReadExpr(arena, source);
      return (FblcExpr*)expr;
    }

    default:
      assert(false && "Invalid expression tag");
      return NULL;
  }
}

// ReadActn --
//   Read an action from a bit source.
//
// Inputs:
//   arena - allocator to use to build the action.
//   source - the bit source to read the action from.
//
// Results:
//   The action read from the bit source.
//
// Side effects:
//   Bits are read from the bit source.
static FblcActn* ReadActn(FblcArena* arena, BitSource* source)
{
  switch (ReadBits(source, 3)) {
    case FBLC_EVAL_ACTN: {
      FblcEvalActn* actn = arena->alloc(arena, sizeof(FblcEvalActn));
      actn->tag = FBLC_EVAL_ACTN;
      actn->expr = ReadExpr(arena, source);
      return (FblcActn*)actn;
    }

    case FBLC_GET_ACTN: {
      FblcGetActn* actn = arena->alloc(arena, sizeof(FblcGetActn));
      actn->tag = FBLC_GET_ACTN;
      actn->port = ReadId(source);
      return (FblcActn*)actn;
    }

    case FBLC_PUT_ACTN: {
      FblcPutActn* actn = arena->alloc(arena, sizeof(FblcPutActn));
      actn->tag = FBLC_PUT_ACTN;
      actn->port = ReadId(source);
      actn->arg = ReadExpr(arena, source);
      return (FblcActn*)actn;
    }

    case FBLC_COND_ACTN: {
      FblcCondActn* actn = arena->alloc(arena, sizeof(FblcCondActn));
      actn->tag = FBLC_COND_ACTN;
      actn->select = ReadExpr(arena, source);
      FblcVectorInit(arena, actn->argv, actn->argc);
      do { 
        FblcVectorAppend(arena, actn->argv, actn->argc, ReadActn(arena, source));
      } while (ReadBits(source, 1));
      return (FblcActn*)actn;
    }

    case FBLC_CALL_ACTN: {
      FblcCallActn* actn = arena->alloc(arena, sizeof(FblcCallActn));
      actn->tag = FBLC_CALL_ACTN;
      actn->proc = ReadId(source);
      FblcVectorInit(arena, actn->portv, actn->portc);
      while (ReadBits(source, 1)) {
        FblcVectorAppend(arena, actn->portv, actn->portc, ReadId(source));
      }
      FblcVectorInit(arena, actn->argv, actn->argc);
      while (ReadBits(source, 1)) {
        FblcVectorAppend(arena, actn->argv, actn->argc, ReadExpr(arena, source));
      }
      return (FblcActn*)actn;
    }

    case FBLC_LINK_ACTN: {
      FblcLinkActn* actn = arena->alloc(arena, sizeof(FblcLinkActn));
      actn->tag = FBLC_LINK_ACTN;
      actn->type = ReadId(source);
      actn->body = ReadActn(arena, source);
      return (FblcActn*)actn;
    }

    case FBLC_EXEC_ACTN: {
      FblcExecActn* actn = arena->alloc(arena, sizeof(FblcExecActn));
      actn->tag = FBLC_EXEC_ACTN;
      FblcVectorInit(arena, actn->execv, actn->execc);
      do { 
        FblcVectorAppend(arena, actn->execv, actn->execc, ReadActn(arena, source));
      } while (ReadBits(source, 1));
      actn->body = ReadActn(arena, source);
      return (FblcActn*)actn;
    }

    default:
      assert(false && "Invalid action tag");
      return NULL;
  }
}

// ReadDecl --
//   Read a declaration from a bit source.
//
// Inputs:
//   arena - allocator to use to build the declaration.
//   source - the bit source to read the declaration from.
//
// Results:
//   The declaration read from the bit source.
//
// Side effects:
//   Bits are read from the bit source.
static FblcDecl* ReadDecl(FblcArena* arena, BitSource* source)
{
  switch (ReadBits(source, 2)) {
    case FBLC_STRUCT_DECL: {
      FblcTypeDecl* decl = arena->alloc(arena, sizeof(FblcTypeDecl));
      decl->tag = FBLC_STRUCT_DECL;
      FblcVectorInit(arena, decl->fieldv, decl->fieldc);
      while (ReadBits(source, 1)) {
        FblcVectorAppend(arena, decl->fieldv, decl->fieldc, ReadId(source));
      }
      return (FblcDecl*)decl;
    }

    case FBLC_UNION_DECL: {
      FblcTypeDecl* decl = arena->alloc(arena, sizeof(FblcTypeDecl));
      decl->tag = FBLC_UNION_DECL;
      FblcVectorInit(arena, decl->fieldv, decl->fieldc);
      do {
        FblcVectorAppend(arena, decl->fieldv, decl->fieldc, ReadId(source));
      } while (ReadBits(source, 1));
      return (FblcDecl*)decl;
    }

    case FBLC_FUNC_DECL: {
      FblcFuncDecl* decl = arena->alloc(arena, sizeof(FblcFuncDecl));
      decl->tag = FBLC_FUNC_DECL;
      FblcVectorInit(arena, decl->argv, decl->argc);
      while (ReadBits(source, 1)) {
        FblcVectorAppend(arena, decl->argv, decl->argc, ReadId(source));
      }
      decl->return_type = ReadId(source);
      decl->body = ReadExpr(arena, source);
      return (FblcDecl*)decl;
    }

    case FBLC_PROC_DECL: {
      FblcProcDecl* decl = arena->alloc(arena, sizeof(FblcProcDecl));
      decl->tag = FBLC_PROC_DECL;
      FblcVectorInit(arena, decl->portv, decl->portc);
      while (ReadBits(source, 1)) {
        FblcVectorExtend(arena, decl->portv, decl->portc);
        decl->portv[decl->portc].type = ReadId(source);
        decl->portv[decl->portc++].polarity = ReadBits(source, 1);
      }
      FblcVectorInit(arena, decl->argv, decl->argc);
      while (ReadBits(source, 1)) {
        FblcVectorAppend(arena, decl->argv, decl->argc, ReadId(source));
      }
      decl->return_type = ReadId(source);
      decl->body = ReadActn(arena, source);
      return (FblcDecl*)decl;
    }

    default:
      assert(false && "invalid decl tag");
      return NULL;
  }
}

// ReadProgram --
//   Read an fblc program from a bit source.
//
// Inputs:
//   arena - allocator to use to build the program.
//   source - the bit source to read the program from.
//
// Results:
//   The program read from the bit source.
//
// Side effects:
//   Bits are read from the bit source.
static FblcProgram* ReadProgram(FblcArena* arena, BitSource* source)
{
  FblcProgram* program = arena->alloc(arena, sizeof(FblcProgram));
  FblcVectorInit(arena, program->declv, program->declc);
  do {
    FblcVectorAppend(arena, program->declv, program->declc, ReadDecl(arena, source));
  } while (ReadBits(source, 1));
  return program;
}

// FblcReadProgram -- see documentation in fblc.h
FblcProgram* FblcReadProgram(FblcArena* arena, int fd)
{
  BitSource source = { .string = NULL, .fd = fd, .synced = false };
  return ReadProgram(arena, &source);
}

// TagSize --
//   Compute the number of bits in a tag for a struct or union with the given
//   number of fields.
//
// Inputs:
//   fieldc - The number of fields to compute the tag size of.
//
// Returns:
//   The size in bits of a tag for the given number of fields.
//
// Side effects:
//   None.
static size_t TagSize(size_t fieldc)
{
  size_t size = 0;
  while ((1 << size) < fieldc) {
    ++size;
  }
  return size;
}

// ReadValue --
//   Read an FblcValue from a bit source.
//
// Inputs:
//   arena - Arena used for allocations.
//   source - The bit source to read the value from.
//   prg - The program containing the type description for the value.
//   type - The id of the type of value to read.
//
// Results:
//   The value read from the bit source.
//
// Side effects:
//   Reads bits from the bit source.
//   Performs arena allocations.
static FblcValue* ReadValue(FblcArena* arena, BitSource* source, FblcProgram* prg, FblcTypeId type)
{
  FblcDecl* decl = prg->declv[type];
  switch (decl->tag) {
    case FBLC_STRUCT_DECL: {
      FblcTypeDecl* struct_decl = (FblcTypeDecl*)decl;
      FblcValue* value = FblcNewStruct(arena, struct_decl->fieldc);
      for (size_t i = 0; i < struct_decl->fieldc; ++i) {
        value->fields[i] = ReadValue(arena, source, prg, struct_decl->fieldv[i]);
      }
      return value;
    }

    case FBLC_UNION_DECL: {
      FblcTypeDecl* union_decl = (FblcTypeDecl*)decl;
      size_t tag = ReadBits(source, TagSize(union_decl->fieldc));
      FblcValue* field = ReadValue(arena, source, prg, union_decl->fieldv[tag]);
      return FblcNewUnion(arena, union_decl->fieldc, tag, field);
    }

    default:
      assert(false && "type id does not refer to a type declaration");
      return NULL;
  }
}

// FblcReadValue -- see documentation in fblc.h
FblcValue* FblcReadValue(FblcArena* arena, FblcProgram* prg, FblcTypeId type, int fd)
{
  BitSource source = { .string = NULL, .fd = fd, .synced = false };
  FblcValue* value = ReadValue(arena, &source, prg, type);
  SyncBitSource(&source);
  return value;
}

// FblcReadValueFromString -- see documentation in fblc.h
FblcValue* FblcReadValueFromString(FblcArena* arena, FblcProgram* prg, FblcTypeId type, const char* string)
{
  BitSource source = { .string = string, .fd = -1, .synced = false };
  FblcValue* value = ReadValue(arena, &source, prg, type);
  SyncBitSource(&source);
  return value;
}

// WriteValue --
//   Write an FblcValue to a bit sink.
//
// Inputs:
//   sink - The bit sink to write the value to.
//   value - The value to write.
//
// Results:
//   None.
//
// Side effects:
//   The value is written to the bit sink.
static void WriteValue(BitSink* sink, FblcValue* value)
{
  switch (value->kind) {
    case FBLC_STRUCT_KIND:
      for (size_t i = 0; i < value->fieldc; ++i) {
        WriteValue(sink, value->fields[i]);
      }
      break;

    case FBLC_UNION_KIND:
      WriteBits(sink, TagSize(value->fieldc), value->tag);
      WriteValue(sink, *value->fields);
      break;

    default:
      assert(false && "Unknown value kind");
  }
}

// FblcWriteValue -- see documentation in fblc.h
void FblcWriteValue(FblcValue* value, int fd)
{
  BitSink sink = { .fd = fd, .synced = false };
  WriteValue(&sink, value);
  SyncBitSink(&sink);
}
