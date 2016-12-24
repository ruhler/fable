
#include <assert.h>
#include <string.h>

#include "fblc.h"

static size_t DecodeId(InputBitStream* bits)
{
  switch (ReadBits(bits, 2)) {
    case 0: return 0;
    case 1: return 1;
    case 2: return 2 * DecodeId(bits);
    case 3: return 2 * DecodeId(bits) + 1;
    default: assert(false && "Unsupported Id tag");
  }
}

static FblcTypeId* DecodeNonEmptyTypes(FblcArena* arena, InputBitStream* bits, size_t* count)
{
  FblcTypeId* types;
  FblcVectorInit(arena, types, *count);
  do {
    FblcVectorAppend(arena, types, *count, DecodeId(bits));
  } while (ReadBits(bits, 1));
  return types;
}

static FblcTypeId* DecodeTypeIds(FblcArena* arena, InputBitStream* bits, size_t* count)
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

static FblcExpr* DecodeExpr(FblcArena* arena, InputBitStream* bits)
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

static FblcPort* DecodePorts(FblcArena* arena, InputBitStream* bits, size_t* count)
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

static FblcActn* DecodeActn(FblcArena* arena, InputBitStream* bits)
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

static FblcDecl* DecodeDecl(FblcArena* arena, InputBitStream* bits)
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

FblcProgram* DecodeProgram(FblcArena* arena, InputBitStream* bits)
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

Value* DecodeValue(FblcArena* arena, InputBitStream* bits, FblcProgram* prg, FblcTypeId type)
{
  FblcDecl* decl = prg->declv[type];
  switch (decl->tag) {
    case FBLC_STRUCT_DECL: {
      FblcTypeDecl* struct_decl = (FblcTypeDecl*)decl;
      Value* value = NewStruct(arena, struct_decl->fieldc);
      for (size_t i = 0; i < struct_decl->fieldc; ++i) {
        value->fields[i] = DecodeValue(arena, bits, prg, struct_decl->fieldv[i]);
      }
      return value;
    }

    case FBLC_UNION_DECL: {
      FblcTypeDecl* union_decl = (FblcTypeDecl*)decl;
      size_t tag = ReadBits(bits, SizeOfTag(union_decl->fieldc));
      Value* field = DecodeValue(arena, bits, prg, union_decl->fieldv[tag]);
      return NewUnion(arena, union_decl->fieldc, tag, field);
    }

    default:
      assert(false && "type id does not refer to a type declaration");
      return NULL;
  }
}

void EncodeValue(OutputBitStream* bits, Value* value)
{
  switch (value->kind) {
    case STRUCT_KIND:
      for (size_t i = 0; i < value->fieldc; ++i) {
        EncodeValue(bits, value->fields[i]);
      }
      break;

    case UNION_KIND:
      WriteBits(bits, SizeOfTag(value->fieldc), value->tag);
      EncodeValue(bits, *value->fields);
      break;

    default:
      assert(false && "Unknown value kind");
  }
}
