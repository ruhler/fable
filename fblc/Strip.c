// Strip.c --
//
//   This file implements routines for converting an fblc text program to an
//   fblc machine program.

#include <assert.h>     // for assert

#include "fblct.h"

static FblcDecl* StripDecl(FblcArena* arena, Decl* tdecl)
{
  switch (tdecl->tag) {
    case FBLC_STRUCT_DECL:
    case FBLC_UNION_DECL: return (FblcDecl*)tdecl;

    case FBLC_FUNC_DECL: {
      FuncDecl* tfunc = (FuncDecl*)tdecl;
      FblcFuncDecl* decl = arena->alloc(arena, sizeof(FblcFuncDecl));
      decl->tag = FBLC_FUNC_DECL;
      FblcVectorInit(arena, decl->argv, decl->argc);
      for (size_t i = 0; i < tfunc->argc; ++i) {
        FblcVectorAppend(arena, decl->argv, decl->argc, tfunc->argv[i]);
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
        FblcVectorAppend(arena, decl->argv, decl->argc, tproc->argv[i]);
      }
      decl->return_type = tproc->return_type_id;
      decl->body = (FblcActn*)tproc->body;
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
