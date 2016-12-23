
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

static TypeId* DecodeNonEmptyTypes(FblcArena* arena, InputBitStream* bits, size_t* count)
{
  TypeId* types;
  FblcVectorInit(arena, types, *count);
  do {
    FblcVectorAppend(arena, types, *count, DecodeId(bits));
  } while (ReadBits(bits, 1));
  return types;
}

static TypeId* DecodeTypeIds(FblcArena* arena, InputBitStream* bits, size_t* count)
{
  TypeId* types;
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

static Expr* DecodeExpr(FblcArena* arena, InputBitStream* bits)
{
  switch (ReadBits(bits, 3)) {
    case VAR_EXPR: {
      VarExpr* expr = arena->alloc(arena, sizeof(VarExpr));
      expr->tag = VAR_EXPR;
      expr->var = DecodeId(bits);
      return (Expr*)expr;
    }

    case APP_EXPR: {
      AppExpr* expr = arena->alloc(arena, sizeof(AppExpr));
      expr->tag = APP_EXPR;
      expr->func = DecodeId(bits);
      FblcVectorInit(arena, expr->argv, expr->argc);
      while (ReadBits(bits, 1)) {
        FblcVectorAppend(arena, expr->argv, expr->argc, DecodeExpr(arena, bits));
      }
      return (Expr*)expr;
    }

    case UNION_EXPR: {
      UnionExpr* expr = arena->alloc(arena, sizeof(UnionExpr));
      expr->tag = UNION_EXPR;
      expr->type = DecodeId(bits);
      expr->field = DecodeId(bits);
      expr->body = DecodeExpr(arena, bits);
      return (Expr*)expr;
    }

    case ACCESS_EXPR: {
     AccessExpr* expr = arena->alloc(arena, sizeof(AccessExpr));
     expr->tag = ACCESS_EXPR;
     expr->object = DecodeExpr(arena, bits);
     expr->field = DecodeId(bits);
     return (Expr*)expr;
    }

    case COND_EXPR: {
      CondExpr* expr = arena->alloc(arena, sizeof(CondExpr));
      expr->tag = COND_EXPR;
      expr->select = DecodeExpr(arena, bits);
      FblcVectorInit(arena, expr->argv, expr->argc);
      do { 
        FblcVectorAppend(arena, expr->argv, expr->argc, DecodeExpr(arena, bits));
      } while (ReadBits(bits, 1));
      return (Expr*)expr;
    }

    case LET_EXPR: {
      LetExpr* expr = arena->alloc(arena, sizeof(LetExpr));
      expr->tag = LET_EXPR;
      expr->def = DecodeExpr(arena, bits);
      expr->body = DecodeExpr(arena, bits);
      return (Expr*)expr;
    }

    default:
      assert(false && "Invalid expression tag");
      return NULL;
  }
}

static Port* DecodePorts(FblcArena* arena, InputBitStream* bits, size_t* count)
{
  Port* ports;
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

static Actn* DecodeActn(FblcArena* arena, InputBitStream* bits)
{
  switch (ReadBits(bits, 3)) {
    case EVAL_ACTN: {
      EvalActn* actn = arena->alloc(arena, sizeof(EvalActn));
      actn->tag = EVAL_ACTN;
      actn->expr = DecodeExpr(arena, bits);
      return (Actn*)actn;
    }

    case GET_ACTN: {
      GetActn* actn = arena->alloc(arena, sizeof(GetActn));
      actn->tag = GET_ACTN;
      actn->port = DecodeId(bits);
      return (Actn*)actn;
    }

    case PUT_ACTN: {
      PutActn* actn = arena->alloc(arena, sizeof(PutActn));
      actn->tag = PUT_ACTN;
      actn->port = DecodeId(bits);
      actn->arg = DecodeExpr(arena, bits);
      return (Actn*)actn;
    }

    case COND_ACTN: {
      CondActn* actn = arena->alloc(arena, sizeof(CondActn));
      actn->tag = COND_ACTN;
      actn->select = DecodeExpr(arena, bits);
      FblcVectorInit(arena, actn->argv, actn->argc);
      do { 
        FblcVectorAppend(arena, actn->argv, actn->argc, DecodeActn(arena, bits));
      } while (ReadBits(bits, 1));
      return (Actn*)actn;
    }

    case CALL_ACTN: {
      CallActn* actn = arena->alloc(arena, sizeof(CallActn));
      actn->tag = CALL_ACTN;
      actn->proc = DecodeId(bits);
      FblcVectorInit(arena, actn->portv, actn->portc);
      while (ReadBits(bits, 1)) {
        FblcVectorAppend(arena, actn->portv, actn->portc, DecodeId(bits));
      }
      FblcVectorInit(arena, actn->argv, actn->argc);
      while (ReadBits(bits, 1)) {
        FblcVectorAppend(arena, actn->argv, actn->argc, DecodeExpr(arena, bits));
      }
      return (Actn*)actn;
    }

    case LINK_ACTN: {
      LinkActn* actn = arena->alloc(arena, sizeof(LinkActn));
      actn->tag = LINK_ACTN;
      actn->type = DecodeId(bits);
      actn->body = DecodeActn(arena, bits);
      return (Actn*)actn;
    }

    case EXEC_ACTN: {
      ExecActn* actn = arena->alloc(arena, sizeof(ExecActn));
      actn->tag = EXEC_ACTN;
      FblcVectorInit(arena, actn->execv, actn->execc);
      do { 
        FblcVectorAppend(arena, actn->execv, actn->execc, DecodeActn(arena, bits));
      } while (ReadBits(bits, 1));
      actn->body = DecodeActn(arena, bits);
      return (Actn*)actn;
    }

    default:
      assert(false && "Invalid action tag");
      return NULL;
  }
}

static Decl* DecodeDecl(FblcArena* arena, InputBitStream* bits)
{
  switch (ReadBits(bits, 2)) {
    case STRUCT_DECL: {
      TypeDecl* decl = arena->alloc(arena, sizeof(TypeDecl));
      decl->tag = STRUCT_DECL;
      decl->fieldv = DecodeTypeIds(arena, bits, &(decl->fieldc));
      return (Decl*)decl;
    }

    case UNION_DECL: {
      TypeDecl* decl = arena->alloc(arena, sizeof(TypeDecl));
      decl->tag = UNION_DECL;
      decl->fieldv = DecodeNonEmptyTypes(arena, bits, &(decl->fieldc));
      return (Decl*)decl;
    }

    case FUNC_DECL: {
      FuncDecl* decl = arena->alloc(arena, sizeof(FuncDecl));
      decl->tag = FUNC_DECL;
      decl->argv = DecodeTypeIds(arena, bits, &(decl->argc));
      decl->return_type = DecodeId(bits);
      decl->body = DecodeExpr(arena, bits);
      return (Decl*)decl;
    }

    case PROC_DECL: {
      ProcDecl* decl = arena->alloc(arena, sizeof(ProcDecl));
      decl->tag = PROC_DECL;
      decl->portv = DecodePorts(arena, bits, &(decl->portc));
      decl->argv = DecodeTypeIds(arena, bits, &(decl->argc));
      decl->return_type = DecodeId(bits);
      decl->body = DecodeActn(arena, bits);
      return (Decl*)decl;
    }

    default:
      assert(false && "invalid decl tag");
      return NULL;
  }
}

Program* DecodeProgram(FblcArena* arena, InputBitStream* bits)
{
  Program* program = arena->alloc(arena, sizeof(Program));
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

Value* DecodeValue(FblcArena* arena, InputBitStream* bits, Program* prg, TypeId type)
{
  Decl* decl = prg->declv[type];
  switch (decl->tag) {
    case STRUCT_DECL: {
      TypeDecl* struct_decl = (TypeDecl*)decl;
      size_t fieldc = struct_decl->fieldc;
      size_t size = sizeof(StructValue) + fieldc * sizeof(Value*);
      StructValue* value = arena->alloc(arena, size);
      value->refcount = 1;
      value->kind = fieldc;
      for (size_t i = 0; i < fieldc; ++i) {
        value->fields[i] = DecodeValue(arena, bits, prg, struct_decl->fieldv[i]);
      }
      return (Value*)value;
    }

    case UNION_DECL: {
      TypeDecl* union_decl = (TypeDecl*)decl;
      UnionValue* value = NewUnionValue(arena, union_decl->fieldc);
      value->tag = ReadBits(bits, SizeOfTag(union_decl->fieldc));
      value->field = DecodeValue(arena, bits, prg, union_decl->fieldv[value->tag]);
      return (Value*)value;
    }

    default:
      assert(false && "type id does not refer to a type declaration");
      return NULL;
  }
}

void EncodeValue(OutputBitStream* bits, Value* value)
{
  if (value->kind >= 0) {
    StructValue* struct_value = (StructValue*)value;
    for (size_t i = 0; i < value->kind; ++i) {
      EncodeValue(bits, struct_value->fields[i]);
    }
  } else {
    UnionValue* union_value = (UnionValue*)value;
    WriteBits(bits, SizeOfTag(-value->kind), union_value->tag);
    EncodeValue(bits, union_value->field);
  }
}
