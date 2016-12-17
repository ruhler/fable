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

    case LET_EXPR: {
      LetExpr* let_expr = (LetExpr*)expr;
      EncodeDeclId(stream, let_expr->type.id);
      EncodeExpr(stream, let_expr->def);
      EncodeExpr(stream, let_expr->body);
      break;
    }

    default:
      assert(false && "Invalid expression tag");
      break;
  }
}

static void EncodeFunc(OutputBitStream* stream, FuncDecl* func)
{
  for (size_t i = 0; i < func->argc; ++i) {
    WriteBits(stream, 1, 1);
    EncodeDeclId(stream, func->argv[i].type.id);
  }
  WriteBits(stream, 1, 0);
  EncodeDeclId(stream, func->return_type.id);
  EncodeExpr(stream, func->body);
}

static void EncodeActn(OutputBitStream* stream, Actn* actn)
{
  WriteBits(stream, 3, actn->tag);
  switch (actn->tag) {
    case EVAL_ACTN: {
      EvalActn* eval_actn = (EvalActn*)actn;
      EncodeExpr(stream, eval_actn->expr);
      break;
    }

    case GET_ACTN: {
      GetActn* get_actn = (GetActn*)actn;
      EncodeId(stream, get_actn->port.id);
      break;
    }

    case PUT_ACTN: {
      PutActn* put_actn = (PutActn*)actn;
      EncodeId(stream, put_actn->port.id);
      EncodeExpr(stream, put_actn->expr);
      break;
    }

    case COND_ACTN: {
      CondActn* cond_actn = (CondActn*)actn;
      EncodeExpr(stream, cond_actn->select);
      assert(cond_actn->argc > 0);
      EncodeActn(stream, cond_actn->args[0]);
      for (size_t i = 1; i < cond_actn->argc; ++i) {
        WriteBits(stream, 1, 1);
        EncodeActn(stream, cond_actn->args[i]);
      }
      WriteBits(stream, 1, 0);
      break;
    }

    case CALL_ACTN: {
      CallActn* call_actn = (CallActn*)actn;
      for (size_t i = 0; i < call_actn->portc; ++i) {
        WriteBits(stream, 1, 1);
        EncodeId(stream, call_actn->ports[i].id);
      }
      WriteBits(stream, 1, 0);
      for (size_t i = 0; i < call_actn->exprc; ++i) {
        WriteBits(stream, 1, 1);
        EncodeExpr(stream, call_actn->exprs[i]);
      }
      WriteBits(stream, 1, 0);
      break;
    }

    case LINK_ACTN: {
      LinkActn* link_actn = (LinkActn*)actn;
      EncodeDeclId(stream, link_actn->type.id);
      EncodeActn(stream, link_actn->body);
      break;
    }

    case EXEC_ACTN: {
      ExecActn* exec_actn = (ExecActn*)actn;
      assert(exec_actn->execc > 0);
      EncodeDeclId(stream, exec_actn->execv[0].var.type.id);
      EncodeActn(stream, exec_actn->execv[0].actn);
      for (size_t i = 1; i < exec_actn->execc; ++i) {
        WriteBits(stream, 1, 1);
        EncodeDeclId(stream, exec_actn->execv[i].var.type.id);
        EncodeActn(stream, exec_actn->execv[i].actn);
      }
      WriteBits(stream, 1, 0);
      EncodeActn(stream, exec_actn->body);
      break;
    }

    default:
      assert(false && "Invalid action tag");
      break;
  }
}

static void EncodeProc(OutputBitStream* stream, ProcDecl* proc)
{
  for (size_t i = 0; i < proc->portc; ++i) {
    WriteBits(stream, 1, 1);
    EncodeDeclId(stream, proc->portv[i].type.id);
    EncodeDeclId(stream, proc->portv[i].polarity);
  }
  WriteBits(stream, 1, 0);

  for (size_t i = 0; i < proc->argc; ++i) {
    WriteBits(stream, 1, 1);
    EncodeDeclId(stream, proc->argv[i].type.id);
  }
  WriteBits(stream, 1, 0);

  EncodeDeclId(stream, proc->return_type.id);
  EncodeActn(stream, proc->body);
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
    WriteBits(stream, 2, decl->tag);
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
