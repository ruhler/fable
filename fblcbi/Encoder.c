
#include "Internal.h"

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

static TypeId* DecodeNonEmptyTypes(Allocator* alloc, InputBitStream* bits, size_t* count)
{
  Vector vector;
  VectorInit(alloc, &vector, sizeof(TypeId));
  do {
    TypeId* ptr = VectorAppend(&vector);
    *ptr = DecodeId(bits);
  } while (ReadBits(bits, 1));
  return VectorExtract(&vector, count);
}

static TypeId* DecodeTypes(Allocator* alloc, InputBitStream* bits, size_t* count)
{
  Vector vector;
  VectorInit(alloc, &vector, sizeof(TypeId));
  while (ReadBits(bits, 1)) {
    TypeId* ptr = VectorAppend(&vector);
    *ptr = DecodeId(bits);
  }
  return VectorExtract(&vector, count);
}

// DecodeExpr --
//
//   Read an expression from a bit stream.
//
// Inputs:
//   alloc - allocator to use to build the expression.
//   bits - stream of bits to read the expression from.
//
// Results:
//   The decoded expression.
//
// Side effects:
//   The bit stream is advanced to just past the expression read.

static Expr* DecodeExpr(Allocator* alloc, InputBitStream* bits)
{
  switch (ReadBits(bits, 3)) {
    case VAR_EXPR: {
      VarExpr* expr = Alloc(alloc, sizeof(VarExpr));
      expr->tag = VAR_EXPR;
      expr->var = DecodeId(bits);
      return (Expr*)expr;
    }

    case APP_EXPR: {
      AppExpr* expr = Alloc(alloc, sizeof(AppExpr));
      expr->tag = APP_EXPR;
      expr->func = DecodeId(bits);
      Vector vector;
      VectorInit(alloc, &vector, sizeof(Expr*));
      while (ReadBits(bits, 1)) {
        Expr** ptr = VectorAppend(&vector);
        *ptr = DecodeExpr(alloc, bits);
      }
      expr->argv = VectorExtract(&vector, &expr->argc);
      return (Expr*)expr;
    }

    case UNION_EXPR: {
      UnionExpr* expr = Alloc(alloc, sizeof(UnionExpr));
      expr->tag = UNION_EXPR;
      expr->type = DecodeId(bits);
      expr->field = DecodeId(bits);
      expr->body = DecodeExpr(alloc, bits);
      return (Expr*)expr;
    }

    case ACCESS_EXPR: {
     AccessExpr* expr = Alloc(alloc, sizeof(AccessExpr));
     expr->tag = ACCESS_EXPR;
     expr->object = DecodeExpr(alloc, bits);
     expr->field = DecodeId(bits);
     return (Expr*)expr;
    }

    case COND_EXPR: {
      CondExpr* expr = Alloc(alloc, sizeof(CondExpr));
      expr->tag = COND_EXPR;
      expr->select = DecodeExpr(alloc, bits);
      Vector vector;
      VectorInit(alloc, &vector, sizeof(Expr*));
      do { 
        Expr** ptr = VectorAppend(&vector);
        *ptr = DecodeExpr(alloc, bits);
      } while (ReadBits(bits, 1));
      expr->argv = VectorExtract(&vector, &expr->argc);
      return (Expr*)expr;
    }

    case LET_EXPR: {
      LetExpr* expr = Alloc(alloc, sizeof(LetExpr));
      expr->tag = LET_EXPR;
      expr->type = DecodeId(bits);
      expr->def = DecodeExpr(alloc, bits);
      expr->body = DecodeExpr(alloc, bits);
      return (Expr*)expr;
    }

    default:
      assert(false && "Invalid expression tag");
      return NULL;
  }
}

static Decl* DecodeDecl(Allocator* alloc, InputBitStream* bits)
{
  switch (ReadBits(bits, 2)) {
    case 0: {
      TypeDecl* decl = Alloc(alloc, sizeof(TypeDecl));
      decl->tag = STRUCT_DECL;
      decl->fieldv = DecodeTypes(alloc, bits, &(decl->fieldc));
      return (Decl*)decl;
    }

    case 1: {
      TypeDecl* decl = Alloc(alloc, sizeof(TypeDecl));
      decl->tag = UNION_DECL;
      decl->fieldv = DecodeNonEmptyTypes(alloc, bits, &(decl->fieldc));
      return (Decl*)decl;
    }

    case 2: {
      FuncDecl* decl = Alloc(alloc, sizeof(FuncDecl));
      decl->tag = FUNC_DECL;
      decl->argv = DecodeTypes(alloc, bits, &(decl->argc));
      decl->return_type = DecodeId(bits);
      decl->body = DecodeExpr(alloc, bits);
      return (Decl*)decl;
    }

    case 3:
      assert(false && "TODO: Support ProcDecl tag");
      return NULL;

    default:
      assert(false && "Unsupported Decl tag");
      return NULL;
  }
}

Program* DecodeProgram(Allocator* alloc, InputBitStream* bits)
{
  Vector vector;
  VectorInit(alloc, &vector, sizeof(Decl*));
  do {
    Decl** ptr = VectorAppend(&vector);
    *ptr = DecodeDecl(alloc, bits);
  } while (ReadBits(bits, 1));
  Program* program = Alloc(alloc, sizeof(Program));
  program->declv = VectorExtract(&vector, &(program->declc));
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

Value* DecodeValue(InputBitStream* bits, Program* prg, TypeId type)
{
  Decl* decl = prg->declv[type];
  switch (decl->tag) {
    case STRUCT_DECL: {
      TypeDecl* struct_decl = (TypeDecl*)decl;
      size_t fieldc = struct_decl->fieldc;
      StructValue* value = MALLOC(sizeof(StructValue) + fieldc * sizeof(Value*));
      value->refcount = 1;
      value->kind = fieldc;
      for (size_t i = 0; i < fieldc; ++i) {
        value->fields[i] = DecodeValue(bits, prg, struct_decl->fieldv[i]);
      }
      return (Value*)value;
    }

    case UNION_DECL: {
      TypeDecl* union_decl = (TypeDecl*)decl;
      UnionValue* value = MALLOC(sizeof(UnionValue));
      value->refcount = 1;
      value->kind = UNION_KIND;
      value->tag = ReadBits(bits, SizeOfTag(union_decl->fieldc));
      value->field = DecodeValue(bits, prg, union_decl->fieldv[value->tag]);
      return (Value*)value;
    }

    default:
      assert(false && "type id does not refer to a type declaration");
      return NULL;
  }
}

void EncodeValue(OutputBitStream* bits, Program* prg, TypeId type, Value* value)
{
  Decl* decl = prg->declv[type];
  switch (decl->tag) {
    case STRUCT_DECL: {
      TypeDecl* struct_decl = (TypeDecl*)decl;
      StructValue* struct_value = (StructValue*)value;
      size_t fieldc = struct_decl->fieldc;
      assert(value->kind == fieldc);
      for (size_t i = 0; i < fieldc; ++i) {
        EncodeValue(bits, prg, struct_decl->fieldv[i], struct_value->fields[i]);
      }
      break;
    }

    case UNION_DECL: {
      assert(value->kind == UNION_KIND);
      TypeDecl* union_decl = (TypeDecl*)decl;
      UnionValue* union_value = (UnionValue*)value;
      WriteBits(bits, SizeOfTag(union_decl->fieldc), union_value->tag);
      EncodeValue(bits, prg, union_decl->fieldv[union_value->tag], union_value->field);
      break;
    }

    default:
      assert(false && "type id does not refer to a type declaration");
      break;
  }
}
