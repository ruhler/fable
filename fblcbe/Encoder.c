// Encoder.c --
//
//   This file implements routines for encoding an fblc program in binary.

#include "Internal.h"

static void EncodeId(OutputBitStream* stream, size_t id)
{
  assert(id != UNRESOLVED_ID);
  while (id > 1) {
    WriteBits(stream, 2, 2 + (id % 2));
    id /= 2;
  }
  WriteBits(stream, 2, id);
}

static void EncodeDeclId(OutputBitStream* stream, size_t id)
{
  EncodeId(stream, id);
}

static void EncodeType(OutputBitStream* stream, TypeDecl* type)
{
  bool cons = type->tag == STRUCT_DECL;
  if (type->tag == STRUCT_DECL) {
    WriteBits(stream, 2, 0);
  } else {
    assert(type->tag == UNION_DECL);
    WriteBits(stream, 2, 1);
  }

  for (size_t i = 0; i < type->fieldc; ++i) {
    if (cons) {
      WriteBits(stream, 1, 1);
    }
    cons = true;
    EncodeDeclId(stream, type->fieldv[i].type.id);
  }
  WriteBits(stream, 1, 0);
}

static void EncodeExpr(OutputBitStream* stream, Expr* expr)
{
  WriteBits(stream, 3, expr->tag);
  switch (expr->tag) {
    case VAR_EXPR: {
      VarExpr* var_expr = (VarExpr*)expr;
      EncodeId(stream, var_expr->name.id);
      break;
    }

    case APP_EXPR: {
      AppExpr* app_expr = (AppExpr*)expr;
      EncodeDeclId(stream, app_expr->func.id);
      for (size_t i = 0; i < app_expr->argc; ++i) {
        WriteBits(stream, 1, 1);
        EncodeExpr(stream, app_expr->argv[i]);
      }
      WriteBits(stream, 1, 0);
      break;
    }

    case UNION_EXPR: {
      UnionExpr* union_expr = (UnionExpr*)expr;
      EncodeDeclId(stream, union_expr->type.id);
      EncodeId(stream, union_expr->field.id);
      EncodeExpr(stream, union_expr->value);
      break;
    }

    case ACCESS_EXPR: {
      AccessExpr* access_expr = (AccessExpr*)expr;
      EncodeExpr(stream, access_expr->object);
      EncodeId(stream, access_expr->field.id);
      break;
    }

    case COND_EXPR: {
      CondExpr* cond_expr = (CondExpr*)expr;
      EncodeExpr(stream, cond_expr->select);
      EncodeExpr(stream, cond_expr->argv[0]);
      for (size_t i = 1; i < cond_expr->argc; ++i) {
        WriteBits(stream, 1, 1);
        EncodeExpr(stream, cond_expr->argv[i]);
      }
      WriteBits(stream, 1, 0);
      break;
    }

    case LET_EXPR:
      assert(false && "TODO");
      break;

    default:
      assert(false && "Invalid expression tag");
      break;
  }
}

static void EncodeFunc(OutputBitStream* stream, FuncDecl* func)
{
  WriteBits(stream, 2, 2);
  for (size_t i = 0; i < func->argc; ++i) {
    WriteBits(stream, 1, 1);
    EncodeDeclId(stream, func->argv[i].type.id);
  }
  WriteBits(stream, 1, 0);
  EncodeDeclId(stream, func->return_type.id);
  EncodeExpr(stream, func->body);
}

static void EncodeProc(OutputBitStream* stream, ProcDecl* proc)
{
  assert(false && "TODO");
}

// EncodeProgram -
//
//   Write the given program to the given bit stream.
//
// Inputs:
//   stream - The stream to output the program to.
//   env - The program to write.
//
// Returns:
//   None.
//
// Side effects:
//   The program is written to the bit stream.

void EncodeProgram(OutputBitStream* stream, Env* env)
{
  for (size_t i = 0; i < env->declc; ++i) {
    if (i > 0) {
      WriteBits(stream, 1, 1);
    }
    Decl* decl = env->declv[i];
    switch (decl->tag) {
      case STRUCT_DECL:
      case UNION_DECL:
        EncodeType(stream, (TypeDecl*)decl);
        break;

      case FUNC_DECL:
        EncodeFunc(stream, (FuncDecl*)decl);
        break;

      case PROC_DECL:
        EncodeProc(stream, (ProcDecl*)decl);
        break;

      default:
        assert(false && "Invalid Decl type");
        break;
    }
  }
  WriteBits(stream, 1, 0);
}
