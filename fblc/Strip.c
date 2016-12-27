// Strip.c --
//
//   This file implements routines for converting an fblc text program to an
//   fblc machine program.

#include <assert.h>     // for assert

#include "fblct.h"

static FblcExpr* StripExpr(FblcArena* arena, Expr* texpr)
{
  switch (texpr->tag) {
    case FBLC_VAR_EXPR: {
      VarExpr* tvar = (VarExpr*)texpr;
      FblcVarExpr* expr = arena->alloc(arena, sizeof(FblcVarExpr));
      expr->tag = FBLC_VAR_EXPR;
      expr->var = tvar->var;
      return (FblcExpr*)expr;
    }

    case FBLC_APP_EXPR: {
      AppExpr* tapp = (AppExpr*)texpr;
      FblcAppExpr* expr = arena->alloc(arena, sizeof(FblcAppExpr));
      expr->tag = FBLC_APP_EXPR;
      expr->func = tapp->func_id;
      FblcVectorInit(arena, expr->argv, expr->argc);
      for (size_t i = 0; i < tapp->argc; ++i) {
        FblcVectorAppend(arena, expr->argv, expr->argc, StripExpr(arena, tapp->argv[i]));
      }
      return (FblcExpr*)expr;
    }

    case FBLC_UNION_EXPR: {
      UnionExpr* tunion = (UnionExpr*)texpr;
      FblcUnionExpr* expr = arena->alloc(arena, sizeof(FblcUnionExpr));
      expr->tag = FBLC_UNION_EXPR;
      expr->type = tunion->type_id;
      expr->field = tunion->field_id;
      expr->body = StripExpr(arena, tunion->body);
      return (FblcExpr*)expr;
    }

    case FBLC_ACCESS_EXPR: {
      AccessExpr* taccess = (AccessExpr*)texpr;
      FblcAccessExpr* expr = arena->alloc(arena, sizeof(FblcAccessExpr));
      expr->tag = FBLC_ACCESS_EXPR;
      expr->object = StripExpr(arena, taccess->object);
      expr->field = taccess->field_id;
      return (FblcExpr*)expr;
    }

    case FBLC_COND_EXPR: {
      CondExpr* tcond = (CondExpr*)texpr;
      FblcCondExpr* expr = arena->alloc(arena, sizeof(FblcCondExpr));
      expr->tag = FBLC_COND_EXPR;
      expr->select = StripExpr(arena, tcond->select);
      FblcVectorInit(arena, expr->argv, expr->argc);
      for (size_t i = 0; i < tcond->argc; ++i) {
        FblcVectorAppend(arena, expr->argv, expr->argc, StripExpr(arena, tcond->argv[i]));
      }
      return (FblcExpr*)expr;
    }

    case FBLC_LET_EXPR: {
      LetExpr* tlet = (LetExpr*)texpr;
      FblcLetExpr* expr = arena->alloc(arena, sizeof(FblcLetExpr));
      expr->tag = FBLC_LET_EXPR;
      expr->def = StripExpr(arena, tlet->def);
      expr->body = StripExpr(arena, tlet->body);
      return (FblcExpr*)expr;
    }

    default:
      assert(false && "Invalid expression tag");
      return NULL;
  }
}

static FblcActn* StripActn(FblcArena* arena, Actn* tactn)
{
  switch (tactn->tag) {
    case FBLC_EVAL_ACTN: {
      EvalActn* teval = (EvalActn*)tactn;
      FblcEvalActn* actn = arena->alloc(arena, sizeof(FblcEvalActn));
      actn->tag = FBLC_EVAL_ACTN;
      actn->expr = StripExpr(arena, teval->expr);
      return (FblcActn*)actn;
    }

    case FBLC_GET_ACTN: {
      GetActn* tget = (GetActn*)tactn;
      FblcGetActn* actn = arena->alloc(arena, sizeof(FblcGetActn));
      actn->tag = FBLC_GET_ACTN;
      actn->port = tget->port_id;
      return (FblcActn*)actn;
    }

    case FBLC_PUT_ACTN: {
      PutActn* tput = (PutActn*)tactn;
      FblcPutActn* actn = arena->alloc(arena, sizeof(FblcPutActn));
      actn->tag = FBLC_PUT_ACTN;
      actn->port = tput->port_id;
      actn->arg = StripExpr(arena, tput->expr);
      return (FblcActn*)actn;
    }

    case FBLC_COND_ACTN: {
      CondActn* tcond = (CondActn*)tactn;
      FblcCondActn* actn = arena->alloc(arena, sizeof(FblcCondActn));
      actn->tag = FBLC_COND_ACTN;
      actn->select = StripExpr(arena, tcond->select);
      FblcVectorInit(arena, actn->argv, actn->argc);
      for (size_t i = 0; i < tcond->argc; ++i) {
        FblcVectorAppend(arena, actn->argv, actn->argc, StripActn(arena, tcond->args[i]));
      }
      return (FblcActn*)actn;
    }

    case FBLC_CALL_ACTN: {
      CallActn* tcall = (CallActn*)tactn;
      FblcCallActn* actn = arena->alloc(arena, sizeof(FblcCallActn));
      actn->tag = FBLC_CALL_ACTN;
      actn->proc = tcall->proc_id;
      FblcVectorInit(arena, actn->portv, actn->portc);
      for (size_t i = 0; i < tcall->portc; ++i) {
        FblcVectorAppend(arena, actn->portv, actn->portc, tcall->port_ids[i]);
      }
      FblcVectorInit(arena, actn->argv, actn->argc);
      for (size_t i = 0; i < tcall->exprc; ++i) {
        FblcVectorAppend(arena, actn->argv, actn->argc, StripExpr(arena, tcall->exprs[i]));
      }
      return (FblcActn*)actn;
    }

    case FBLC_LINK_ACTN: {
      LinkActn* tlink = (LinkActn*)tactn;
      FblcLinkActn* actn = arena->alloc(arena, sizeof(FblcLinkActn));
      actn->tag = FBLC_LINK_ACTN;
      actn->type = tlink->type_id;
      actn->body = StripActn(arena, tlink->body);
      return (FblcActn*)actn;
    }

    case FBLC_EXEC_ACTN: {
      ExecActn* texec = (ExecActn*)tactn;
      FblcExecActn* actn = arena->alloc(arena, sizeof(FblcExecActn));
      actn->tag = FBLC_EXEC_ACTN;
      FblcVectorInit(arena, actn->execv, actn->execc);
      for (size_t i = 0; i < texec->execc; ++i) {
        FblcVectorAppend(arena, actn->execv, actn->execc, StripActn(arena, texec->execv[i].actn));
      }
      actn->body = StripActn(arena, texec->body);
      return (FblcActn*)actn;
    }

    default:
      assert(false && "Invalid action tag");
      return NULL;
  }
}

static FblcDecl* StripDecl(FblcArena* arena, Decl* tdecl)
{
  switch (tdecl->tag) {
    case FBLC_STRUCT_DECL:
    case FBLC_UNION_DECL: {
      TypeDecl* ttype = (TypeDecl*)tdecl;
      FblcTypeDecl* decl = arena->alloc(arena, sizeof(FblcTypeDecl));
      decl->tag = ttype->tag;
      FblcVectorInit(arena, decl->fieldv, decl->fieldc);
      for (size_t i = 0; i < ttype->fieldc; ++i) {
        FblcVectorAppend(arena, decl->fieldv, decl->fieldc, ttype->fieldv[i].type_id);
      }
      return (FblcDecl*)decl;
    }

    case FBLC_FUNC_DECL: {
      FuncDecl* tfunc = (FuncDecl*)tdecl;
      FblcFuncDecl* decl = arena->alloc(arena, sizeof(FblcFuncDecl));
      decl->tag = FBLC_FUNC_DECL;
      FblcVectorInit(arena, decl->argv, decl->argc);
      for (size_t i = 0; i < tfunc->argc; ++i) {
        FblcVectorAppend(arena, decl->argv, decl->argc, tfunc->argv[i].type_id);
      }
      decl->return_type = tfunc->return_type_id;
      decl->body = StripExpr(arena, tfunc->body);
      return (FblcDecl*)decl;
    }

    case FBLC_PROC_DECL: {
      ProcDecl* tproc = (ProcDecl*)tdecl;
      FblcProcDecl* decl = arena->alloc(arena, sizeof(FblcProcDecl));
      decl->tag = FBLC_PROC_DECL;
      FblcVectorInit(arena, decl->portv, decl->portc);
      for (size_t i = 0; i < tproc->portc; ++i) {
        FblcVectorExtend(arena, decl->portv, decl->portc);
        decl->portv[decl->portc].type = tproc->portv[i].type_id;
        decl->portv[decl->portc++].polarity = tproc->portv[i].polarity;
      }
      FblcVectorInit(arena, decl->argv, decl->argc);
      for (size_t i = 0; i < tproc->argc; ++i) {
        FblcVectorAppend(arena, decl->argv, decl->argc, tproc->argv[i].type_id);
      }
      decl->return_type = tproc->return_type_id;
      decl->body = StripActn(arena, tproc->body);
      return (FblcDecl*)decl;
    }

    default:
      assert(false && "invalid decl tag");
      return NULL;
  }
}

FblcProgram* StripProgram(FblcArena* arena, Env* tprog)
{
  FblcProgram* prog = arena->alloc(arena, sizeof(FblcProgram));
  FblcVectorInit(arena, prog->declv, prog->declc); 
  for (size_t i = 0; i < tprog->declc; ++i) {
    FblcVectorAppend(arena, prog->declv, prog->declc, StripDecl(arena, tprog->declv[i]));
  }
  return prog;
}
