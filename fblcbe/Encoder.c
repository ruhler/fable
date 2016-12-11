// Encoder.c --
//
//   This file implements routines for encoding an fblc program in binary.

#include "Internal.h"

// IdForDecl -
//   
//   Return the identifier for the declaration with the given name.
//
// Inputs:
//   env - the program environment.
//   name - the name of the declaration to identify.
//
// Returns:
//   The identifier to use for the declaration.
//
// Side effects:
//   None. The behavior is undefined if name does not refer to a declaration
//   in the environment.

static uint32_t IdForDecl(const Env* env, Name name)
{
  // TODO: Linear search through the entire environment for every lookup seems
  // excessively slow. Consider using a more efficient data structure, such as
  // a hash table.

  // Note: The search order here corresponds to the order the declarations are
  // generated in the EncodeProgram function.
  size_t i = 0;
  for (TypeEnv* types = env->types; types != NULL; types = types->next) {
    if (NamesEqual(name, types->decl->name.name)) {
      return i;
    }
    ++i;
  }

  for (FuncEnv* funcs = env->funcs; funcs != NULL; funcs = funcs->next) {
    if (NamesEqual(name, funcs->decl->name.name)) {
      return i;
    }
    ++i;
  }

  for (ProcEnv* procs = env->procs; procs != NULL; procs = procs->next) {
    if (NamesEqual(name, procs->decl->name.name)) {
      return i;
    }
    ++i;
  }

  assert(false && "Name not found in environment.");
  return -1;
}

static void EncodeId(OutputBitStream* stream, size_t id)
{
  while (id > 1) {
    WriteBits(stream, 2, 2 + (id % 2));
    id /= 2;
  }
  WriteBits(stream, 2, id);
}

static void EncodeDeclId(OutputBitStream* stream, const Env* env, Name name)
{
  EncodeId(stream, IdForDecl(env, name));
}

static void EncodeType(OutputBitStream* stream, const Env* env, Type* type)
{
  bool cons = type->kind == KIND_STRUCT;
  if (type->kind == KIND_STRUCT) {
    WriteBits(stream, 2, 0);
  } else {
    assert(type->kind == KIND_UNION);
    WriteBits(stream, 2, 1);
  }

  for (size_t i = 0; i < type->fieldc; ++i) {
    if (cons) {
      WriteBits(stream, 1, 1);
    }
    cons = true;
    EncodeDeclId(stream, env, type->fieldv[i].type.name);
  }
  WriteBits(stream, 1, 0);
}

static void EncodeExpr(OutputBitStream* stream, const Env* env, const Expr* expr)
{
  switch (expr->tag) {
    case VAR_EXPR:
      WriteBits(stream, 3, 0);
      assert(false && "TODO: Map variable name to id");
      break;

    case APP_EXPR: {
      AppExpr* app_expr = (AppExpr*)expr;
      WriteBits(stream, 3, 1);
      EncodeDeclId(stream, env, app_expr->func.name);
      for (size_t i = 0; i < app_expr->argc; ++i) {
        WriteBits(stream, 1, 1);
        EncodeExpr(stream, env, app_expr->argv[i]);
      }
      WriteBits(stream, 1, 0);
      break;
    }

    case ACCESS_EXPR:
      assert(false && "TODO");

    case UNION_EXPR: {
      UnionExpr* union_expr = (UnionExpr*)expr;
      WriteBits(stream, 3, 2);
      EncodeDeclId(stream, env, union_expr->type.name);
      Type* type = LookupType(env, union_expr->type.name);
      assert(type != NULL);
      EncodeId(stream, TagForField(type, union_expr->field.name));
      EncodeExpr(stream, env, union_expr->value);
      break;
    }

    case LET_EXPR:
      assert(false && "TODO");
      break;

    case COND_EXPR:
      assert(false && "TODO");
      break;

    default:
      assert(false && "Invalid expression tag");
      break;
  }
}

static void EncodeFunc(OutputBitStream* stream, const Env* env, Func* func)
{
  WriteBits(stream, 2, 2);
  for (size_t i = 0; i < func->argc; ++i) {
    WriteBits(stream, 1, 1);
    EncodeDeclId(stream, env, func->argv[i].type.name);
  }
  WriteBits(stream, 1, 0);
  EncodeDeclId(stream, env, func->return_type.name);
  EncodeExpr(stream, env, func->body);
}

static void EncodeProc(OutputBitStream* stream, const Env* env, Proc* proc)
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

void EncodeProgram(OutputBitStream* stream, const Env* env)
{
  bool initial = true;
  for (TypeEnv* types = env->types; types != NULL; types = types->next) {
    if (!initial) {
      WriteBits(stream, 1, 1);
    }
    initial = false;
    EncodeType(stream, env, types->decl);
  }

  for (FuncEnv* funcs = env->funcs; funcs != NULL; funcs = funcs->next) {
    assert(!initial);
    WriteBits(stream, 1, 1);
    EncodeFunc(stream, env, funcs->decl);
  }

  for (ProcEnv* procs = env->procs; procs != NULL; procs = procs->next) {
    assert(!initial);
    WriteBits(stream, 1, 1);
    EncodeProc(stream, env, procs->decl);
  }
  WriteBits(stream, 1, 0);
}
