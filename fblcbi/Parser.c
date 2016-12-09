
#include "Shared.h"

static Id ParseId(Allocator* alloc, BitStream* bits)
{
  switch (ReadBits(bits, 2)) {
    case 0: return 0;
    case 1: return 1;
    case 2: return 2 * ParseId(alloc, bits);
    case 3: return 2 * ParseId(alloc, bits) + 1;
    default: assert(false && "Unsupported Id tag");
  }
}

static DeclId ParseDeclId(Allocator* alloc, BitStream* bits)
{
  return ParseId(alloc, bits);
}

static Type* ParseTypes(Allocator* alloc, BitStream* bits, size_t* count)
{
  Vector vector;
  VectorInit(alloc, &vector, sizeof(Type));
  while (ReadBits(bits, 1)) {
    Type* ptr = VectorAppend(&vector);
    *ptr = ParseType(alloc, bits);
  }
  return VectorExtract(&vector, count);
}

static Type ParseType(Allocator* alloc, BitStream* bits)
{
  return ParseId(alloc, bits);
}

static Expr* ParseExprs(Allocator* alloc, BitStream* bits, size_t* count)
{
  Vector vector;
  VectorInit(alloc, &vector, sizeof(Expr*));
  while (ReadBits(bits, 1)) {
    Expr** ptr = VectorAppend(&vector);
    *ptr = ParseExpr(alloc, bits);
  }
  return VectorExtract(&vector, count);
}

static Expr* ParseExpr(Allocator* alloc, BitStream* bits)
{
  switch (ReadBits(bits, 3)) {
    case APP_EXPR: {
      AppExpr* expr = Alloc(alloc, sizeof(AppExpr));
      expr->tag = APP_EXPR;
      expr->func = ParseDeclId(alloc, bits);
      expr->argv = ParseExprs(alloc, bits, &(expr->argc));
      return (Expr*)expr;
    }

    default:
      assert(false && "TODO: Expr type");
      return NULL;
  }
}

static Decl* ParseDecl(Allocator* alloc, BitStream* bits)
{
  switch (ReadBits(bits, 2)) {
    case 0: {
      StructDecl* decl = Alloc(alloc, sizeof(StructDecl));
      decl->tag = STRUCT_DECL;
      decl->fieldv = ParseTypes(alloc, bits, &(decl->fieldc));
      return (Decl*)decl;
    }

    case 1:
      assert(false && "TODO: Support UnionDecl tag");
      return NULL;

    case 2: {
      FuncDecl* decl = Alloc(alloc, sizeof(FuncDecl));
      decl->tag = FUNC_DECL;
      decl->argv = ParseTypes(alloc, bits, *(decl->argc));
      decl->return_type = ParseType(alloc, bits);
      decl->body = ParseExpr(alloc, bits);
      return decl->body == NULL ? NULL : (Decl*)decl;
    }

    case 3:
      assert(false && "TODO: Support ProcDecl tag");
      return NULL;

    default:
      assert(false && "Unsupported Decl tag");
      return NULL;
  }
}

Program* ParseProgram(Allocator* alloc, BitStream* bits)
{
  Vector vector;
  VectorInit(alloc, &vector, sizeof(Decl*));
  do {
    Decl** ptr = VectorAppend(&vector);
    *ptr = ParseDecl(alloc, bits);
  } while (ReadBits(bits, 1));
  Program* program = Alloc(alloc, sizeof(Decls));
  program->decls = VectorExtract(&vector, &(program->declc));
  return program;
}
