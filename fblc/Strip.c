// Strip.c --
//
//   This file implements routines for converting an fblc text program to an
//   fblc machine program.

#include <assert.h>     // for assert

#include "fblct.h"

static FblcActn* StripActn(FblcArena* arena, Actn* tactn)
{
  switch (tactn->tag) {
    case FBLC_COND_ACTN: {
      CondActn* tcond = (CondActn*)tactn;
      FblcCondActn* actn = arena->alloc(arena, sizeof(FblcCondActn));
      actn->tag = FBLC_COND_ACTN;
      actn->select = (FblcExpr*)tcond->select;
      FblcVectorInit(arena, actn->argv, actn->argc);
      for (size_t i = 0; i < tcond->argc; ++i) {
        FblcVectorAppend(arena, actn->argv, actn->argc, StripActn(arena, tcond->args[i]));
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

    default: return (FblcActn*)tactn;
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
      decl->body = (FblcExpr*)tfunc->body;
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
