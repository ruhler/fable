
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "fblc.h"

// BitStream
// Bit streams are represented as sequences of ascii digits '0' and '1'.
// TODO: Support more efficient encodings of bit streams when desired.
// If the string field is non-null, digits are read from the string.
// If the fd field is non-negative, digits are read from the file with that
// descriptor.
typedef struct BitSource BitSource;
BitSource* CreateStringBitSource(FblcArena* arena, const char* string);
BitSource* CreateFdBitSource(FblcArena* arena, int fd);
uint32_t ReadBits(BitSource* source, size_t num_bits);
void SyncBitSource(BitSource* source);
void FreeBitSource(FblcArena* arena, BitSource* source);

typedef struct {
  int fd;
  bool flushed;
} OutputBitStream;

void OpenBinaryOutputBitStream(OutputBitStream* stream, int fd);
void WriteBits(OutputBitStream* stream, size_t num_bits, uint32_t bits);
void FlushWriteBits(OutputBitStream* stream);
struct BitSource {
  const char* string;
  int fd;
  bool synced;
};

BitSource* CreateStringBitSource(FblcArena* arena, const char* string)
{
  BitSource* source = arena->alloc(arena, sizeof(BitSource));
  source->string = string;
  source->fd = -1;
  source->synced = false;
  return source;
}

BitSource* CreateFdBitSource(FblcArena* arena, int fd)
{
  BitSource* source = arena->alloc(arena, sizeof(BitSource));
  source->string = NULL;
  source->fd = fd;
  source->synced = false;
  return source;
}

// ReadBit --
//   Read the next bit from the given bit stream.
//
// Inputs:
//   stream - the bit stream to read from.
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
//
//   Read num_bits bits from the given bit stream.
//
// Inputs:
//   stream - the bit stream to read from.
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

uint32_t ReadBits(BitSource* source, size_t num_bits)
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
void SyncBitSource(BitSource* source)
{
  if (!source->synced) {
    ReadBit(source);
  }
}
void FreeBitSource(FblcArena* arena, BitSource* source)
{
  arena->free(arena, source);
}

// OpenBinaryOutputBitStream
//   Open an OutputBitStream that writes ascii digits '0' and '1' to an open
//   file.
//
// Inputs:
//   stream - The stream to open.
//   fd - The file descriptor of an open file to write to.
//
// Returns:
//   None.
//
// Side effects:
//   The bit stream set to write to the given file.

void OpenBinaryOutputBitStream(OutputBitStream* stream, int fd)
{
  stream->fd = fd;
  stream->flushed = false;
}

// WriteBits --
//
//   Write num_bits bits to the given bit stream.
//
// Inputs:
//   stream - the bit stream to write to.
//   num_bits - the number of bits to write. This must be less than 32.
//   bits - the actual bits to write.
//
// Returns:
//   None.
//
// Side effects:
//   num_bits bits are written to the stream in the form of binary ascii
//   digits '0' and '1'.
void WriteBits(OutputBitStream* stream, size_t num_bits, uint32_t bits)
{
  assert(num_bits < 32 && "WriteBits invalid num_bits");
  for (uint32_t mask = ((1 << num_bits) >> 1); mask > 0; mask >>= 1) {
    char c = (bits & mask) ? '1' : '0';
    write(stream->fd, &c, 1);
    stream->flushed = true;
  }
}

// FlushWriteBits --
//
//   Flush bits as necessary to mark the end of a value.
//
// Inputs:
//   stream - the bit stream to flush the bits in.
//
// Returns:
//   None.
//
// Side effects:
//   If necessary, write padding bits to the stream to indicate the end of a
//   value has been reached.
void FlushWriteBits(OutputBitStream* stream)
{
  if (!stream->flushed) {
    WriteBits(stream, 1, 0);
  }
}


static size_t DecodeId(BitSource* bits)
{
  switch (ReadBits(bits, 2)) {
    case 0: return 0;
    case 1: return 1;
    case 2: return 2 * DecodeId(bits);
    case 3: return 2 * DecodeId(bits) + 1;
    default: assert(false && "Unsupported Id tag");
  }
}

static FblcTypeId* DecodeNonEmptyTypes(FblcArena* arena, BitSource* bits, size_t* count)
{
  FblcTypeId* types;
  FblcVectorInit(arena, types, *count);
  do {
    FblcVectorAppend(arena, types, *count, DecodeId(bits));
  } while (ReadBits(bits, 1));
  return types;
}

static FblcTypeId* DecodeTypeIds(FblcArena* arena, BitSource* bits, size_t* count)
{
  FblcTypeId* types;
  FblcVectorInit(arena, types, *count);
  while (ReadBits(bits, 1)) {
    FblcVectorAppend(arena, types, *count, DecodeId(bits));
  }
  return types;
}

// DecodeExpr --
//
//   Read an expression from a bit stream.
//
// Inputs:
//   arena - allocator to use to build the expression.
//   bits - stream of bits to read the expression from.
//
// Results:
//   The decoded expression.
//
// Side effects:
//   The bit stream is advanced to just past the expression read.

static FblcExpr* DecodeExpr(FblcArena* arena, BitSource* bits)
{
  switch (ReadBits(bits, 3)) {
    case FBLC_VAR_EXPR: {
      FblcVarExpr* expr = arena->alloc(arena, sizeof(FblcVarExpr));
      expr->tag = FBLC_VAR_EXPR;
      expr->var = DecodeId(bits);
      return (FblcExpr*)expr;
    }

    case FBLC_APP_EXPR: {
      FblcAppExpr* expr = arena->alloc(arena, sizeof(FblcAppExpr));
      expr->tag = FBLC_APP_EXPR;
      expr->func = DecodeId(bits);
      FblcVectorInit(arena, expr->argv, expr->argc);
      while (ReadBits(bits, 1)) {
        FblcVectorAppend(arena, expr->argv, expr->argc, DecodeExpr(arena, bits));
      }
      return (FblcExpr*)expr;
    }

    case FBLC_UNION_EXPR: {
      FblcUnionExpr* expr = arena->alloc(arena, sizeof(FblcUnionExpr));
      expr->tag = FBLC_UNION_EXPR;
      expr->type = DecodeId(bits);
      expr->field = DecodeId(bits);
      expr->body = DecodeExpr(arena, bits);
      return (FblcExpr*)expr;
    }

    case FBLC_ACCESS_EXPR: {
     FblcAccessExpr* expr = arena->alloc(arena, sizeof(FblcAccessExpr));
     expr->tag = FBLC_ACCESS_EXPR;
     expr->object = DecodeExpr(arena, bits);
     expr->field = DecodeId(bits);
     return (FblcExpr*)expr;
    }

    case FBLC_COND_EXPR: {
      FblcCondExpr* expr = arena->alloc(arena, sizeof(FblcCondExpr));
      expr->tag = FBLC_COND_EXPR;
      expr->select = DecodeExpr(arena, bits);
      FblcVectorInit(arena, expr->argv, expr->argc);
      do { 
        FblcVectorAppend(arena, expr->argv, expr->argc, DecodeExpr(arena, bits));
      } while (ReadBits(bits, 1));
      return (FblcExpr*)expr;
    }

    case FBLC_LET_EXPR: {
      FblcLetExpr* expr = arena->alloc(arena, sizeof(FblcLetExpr));
      expr->tag = FBLC_LET_EXPR;
      expr->def = DecodeExpr(arena, bits);
      expr->body = DecodeExpr(arena, bits);
      return (FblcExpr*)expr;
    }

    default:
      assert(false && "Invalid expression tag");
      return NULL;
  }
}

static FblcPort* DecodePorts(FblcArena* arena, BitSource* bits, size_t* count)
{
  FblcPort* ports;
  FblcVectorInit(arena, ports, *count);
  while (ReadBits(bits, 1)) {
    FblcVectorExtend(arena, ports, *count);
    ports[*count].type = DecodeId(bits);
    ports[(*count)++].polarity = ReadBits(bits, 1);
  }
  return ports;
}

// DecodeActn --
//
//   Read an action from a bit stream.
//
// Inputs:
//   arena - allocator to use to build the action.
//   bits - stream of bits to read the action from.
//
// Results:
//   The decoded action.
//
// Side effects:
//   The bit stream is advanced to just past the action read.

static FblcActn* DecodeActn(FblcArena* arena, BitSource* bits)
{
  switch (ReadBits(bits, 3)) {
    case FBLC_EVAL_ACTN: {
      FblcEvalActn* actn = arena->alloc(arena, sizeof(FblcEvalActn));
      actn->tag = FBLC_EVAL_ACTN;
      actn->expr = DecodeExpr(arena, bits);
      return (FblcActn*)actn;
    }

    case FBLC_GET_ACTN: {
      FblcGetActn* actn = arena->alloc(arena, sizeof(FblcGetActn));
      actn->tag = FBLC_GET_ACTN;
      actn->port = DecodeId(bits);
      return (FblcActn*)actn;
    }

    case FBLC_PUT_ACTN: {
      FblcPutActn* actn = arena->alloc(arena, sizeof(FblcPutActn));
      actn->tag = FBLC_PUT_ACTN;
      actn->port = DecodeId(bits);
      actn->arg = DecodeExpr(arena, bits);
      return (FblcActn*)actn;
    }

    case FBLC_COND_ACTN: {
      FblcCondActn* actn = arena->alloc(arena, sizeof(FblcCondActn));
      actn->tag = FBLC_COND_ACTN;
      actn->select = DecodeExpr(arena, bits);
      FblcVectorInit(arena, actn->argv, actn->argc);
      do { 
        FblcVectorAppend(arena, actn->argv, actn->argc, DecodeActn(arena, bits));
      } while (ReadBits(bits, 1));
      return (FblcActn*)actn;
    }

    case FBLC_CALL_ACTN: {
      FblcCallActn* actn = arena->alloc(arena, sizeof(FblcCallActn));
      actn->tag = FBLC_CALL_ACTN;
      actn->proc = DecodeId(bits);
      FblcVectorInit(arena, actn->portv, actn->portc);
      while (ReadBits(bits, 1)) {
        FblcVectorAppend(arena, actn->portv, actn->portc, DecodeId(bits));
      }
      FblcVectorInit(arena, actn->argv, actn->argc);
      while (ReadBits(bits, 1)) {
        FblcVectorAppend(arena, actn->argv, actn->argc, DecodeExpr(arena, bits));
      }
      return (FblcActn*)actn;
    }

    case FBLC_LINK_ACTN: {
      FblcLinkActn* actn = arena->alloc(arena, sizeof(FblcLinkActn));
      actn->tag = FBLC_LINK_ACTN;
      actn->type = DecodeId(bits);
      actn->body = DecodeActn(arena, bits);
      return (FblcActn*)actn;
    }

    case FBLC_EXEC_ACTN: {
      FblcExecActn* actn = arena->alloc(arena, sizeof(FblcExecActn));
      actn->tag = FBLC_EXEC_ACTN;
      FblcVectorInit(arena, actn->execv, actn->execc);
      do { 
        FblcVectorAppend(arena, actn->execv, actn->execc, DecodeActn(arena, bits));
      } while (ReadBits(bits, 1));
      actn->body = DecodeActn(arena, bits);
      return (FblcActn*)actn;
    }

    default:
      assert(false && "Invalid action tag");
      return NULL;
  }
}

static FblcDecl* DecodeDecl(FblcArena* arena, BitSource* bits)
{
  switch (ReadBits(bits, 2)) {
    case FBLC_STRUCT_DECL: {
      FblcTypeDecl* decl = arena->alloc(arena, sizeof(FblcTypeDecl));
      decl->tag = FBLC_STRUCT_DECL;
      decl->fieldv = DecodeTypeIds(arena, bits, &(decl->fieldc));
      return (FblcDecl*)decl;
    }

    case FBLC_UNION_DECL: {
      FblcTypeDecl* decl = arena->alloc(arena, sizeof(FblcTypeDecl));
      decl->tag = FBLC_UNION_DECL;
      decl->fieldv = DecodeNonEmptyTypes(arena, bits, &(decl->fieldc));
      return (FblcDecl*)decl;
    }

    case FBLC_FUNC_DECL: {
      FblcFuncDecl* decl = arena->alloc(arena, sizeof(FblcFuncDecl));
      decl->tag = FBLC_FUNC_DECL;
      decl->argv = DecodeTypeIds(arena, bits, &(decl->argc));
      decl->return_type = DecodeId(bits);
      decl->body = DecodeExpr(arena, bits);
      return (FblcDecl*)decl;
    }

    case FBLC_PROC_DECL: {
      FblcProcDecl* decl = arena->alloc(arena, sizeof(FblcProcDecl));
      decl->tag = FBLC_PROC_DECL;
      decl->portv = DecodePorts(arena, bits, &(decl->portc));
      decl->argv = DecodeTypeIds(arena, bits, &(decl->argc));
      decl->return_type = DecodeId(bits);
      decl->body = DecodeActn(arena, bits);
      return (FblcDecl*)decl;
    }

    default:
      assert(false && "invalid decl tag");
      return NULL;
  }
}

FblcProgram* DecodeProgram(FblcArena* arena, BitSource* bits)
{
  FblcProgram* program = arena->alloc(arena, sizeof(FblcProgram));
  FblcVectorInit(arena, program->declv, program->declc);
  do {
    FblcVectorAppend(arena, program->declv, program->declc, DecodeDecl(arena, bits));
  } while (ReadBits(bits, 1));
  return program;
}

static size_t SizeOfTag(size_t count)
{
  size_t size = 0;
  while ((1 << size) < count) {
    ++size;
  }
  return size;
}

FblcValue* DecodeValue(FblcArena* arena, BitSource* bits, FblcProgram* prg, FblcTypeId type)
{
  FblcDecl* decl = prg->declv[type];
  switch (decl->tag) {
    case FBLC_STRUCT_DECL: {
      FblcTypeDecl* struct_decl = (FblcTypeDecl*)decl;
      FblcValue* value = FblcNewStruct(arena, struct_decl->fieldc);
      for (size_t i = 0; i < struct_decl->fieldc; ++i) {
        value->fields[i] = DecodeValue(arena, bits, prg, struct_decl->fieldv[i]);
      }
      return value;
    }

    case FBLC_UNION_DECL: {
      FblcTypeDecl* union_decl = (FblcTypeDecl*)decl;
      size_t tag = ReadBits(bits, SizeOfTag(union_decl->fieldc));
      FblcValue* field = DecodeValue(arena, bits, prg, union_decl->fieldv[tag]);
      return FblcNewUnion(arena, union_decl->fieldc, tag, field);
    }

    default:
      assert(false && "type id does not refer to a type declaration");
      return NULL;
  }
}

void EncodeValue(OutputBitStream* bits, FblcValue* value)
{
  switch (value->kind) {
    case FBLC_STRUCT_KIND:
      for (size_t i = 0; i < value->fieldc; ++i) {
        EncodeValue(bits, value->fields[i]);
      }
      break;

    case FBLC_UNION_KIND:
      WriteBits(bits, SizeOfTag(value->fieldc), value->tag);
      EncodeValue(bits, *value->fields);
      break;

    default:
      assert(false && "Unknown value kind");
  }
}
FblcProgram* FblcReadProgram(FblcArena* arena, int fd)
{
  BitSource* bits = CreateFdBitSource(arena, fd);
  FblcProgram* prg = DecodeProgram(arena, bits);
  FreeBitSource(arena, bits);
  return prg;
}

FblcValue* FblcReadValue(FblcArena* arena, FblcProgram* prg, FblcTypeId type, int fd)
{
  BitSource* source = CreateFdBitSource(arena, fd);
  FblcValue* value = DecodeValue(arena, source, prg, type);
  SyncBitSource(source);
  FreeBitSource(arena, source);
  return value;
}
FblcValue* FblcReadValueFromString(FblcArena* arena, FblcProgram* prg, FblcTypeId type, const char* string)
{
    BitSource* bits = CreateStringBitSource(arena, string);
    FblcValue* value = DecodeValue(arena, bits, prg, type);
    SyncBitSource(bits);
    FreeBitSource(arena, bits);
    return value;
}
void FblcWriteValue(FblcValue* value, int fd)
{
  OutputBitStream stream;
  OpenBinaryOutputBitStream(&stream, fd);
  EncodeValue(&stream, value);
  FlushWriteBits(&stream);
}
