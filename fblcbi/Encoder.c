
#include "Internal.h"

static Expr** DecodeExprs(Allocator* alloc, InputBitStream* bits, size_t* count);


static Id DecodeId(Allocator* alloc, InputBitStream* bits)
{
  switch (ReadBits(bits, 2)) {
    case 0: return 0;
    case 1: return 1;
    case 2: return 2 * DecodeId(alloc, bits);
    case 3: return 2 * DecodeId(alloc, bits) + 1;
    default: assert(false && "Unsupported Id tag");
  }
}

static DeclId DecodeDeclId(Allocator* alloc, InputBitStream* bits)
{
  return DecodeId(alloc, bits);
}

static Type DecodeType(Allocator* alloc, InputBitStream* bits)
{
  return DecodeId(alloc, bits);
}

static Type* DecodeTypes(Allocator* alloc, InputBitStream* bits, size_t* count)
{
  Vector vector;
  VectorInit(alloc, &vector, sizeof(Type));
  while (ReadBits(bits, 1)) {
    Type* ptr = VectorAppend(&vector);
    *ptr = DecodeType(alloc, bits);
  }
  return VectorExtract(&vector, count);
}

static Expr* DecodeExpr(Allocator* alloc, InputBitStream* bits)
{
  switch (ReadBits(bits, 3)) {
    case APP_EXPR: {
      AppExpr* expr = Alloc(alloc, sizeof(AppExpr));
      expr->tag = APP_EXPR;
      expr->func = DecodeDeclId(alloc, bits);
      expr->argv = DecodeExprs(alloc, bits, &(expr->argc));
      return (Expr*)expr;
    }

    default:
      assert(false && "TODO: Expr type");
      return NULL;
  }
}

static Expr** DecodeExprs(Allocator* alloc, InputBitStream* bits, size_t* count)
{
  Vector vector;
  VectorInit(alloc, &vector, sizeof(Expr*));
  while (ReadBits(bits, 1)) {
    Expr** ptr = VectorAppend(&vector);
    *ptr = DecodeExpr(alloc, bits);
  }
  return VectorExtract(&vector, count);
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
      decl->fieldv = DecodeTypes(alloc, bits, &(decl->fieldc));
      return (Decl*)decl;
    }

    case 2: {
      FuncDecl* decl = Alloc(alloc, sizeof(FuncDecl));
      decl->tag = FUNC_DECL;
      decl->argv = DecodeTypes(alloc, bits, &(decl->argc));
      decl->return_type = DecodeType(alloc, bits);
      decl->body = DecodeExpr(alloc, bits);
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

Value* DecodeValue(Allocator* alloc, InputBitStream* bits, Program* prg, Type type)
{
  Decl* decl = prg->declv[type];
  switch (decl->tag) {
    case STRUCT_DECL: {
      TypeDecl* struct_decl = (TypeDecl*)decl;
      size_t fieldc = struct_decl->fieldc;
      StructValue* value = Alloc(alloc, sizeof(StructValue) + fieldc * sizeof(Value*));
      value->refcount = 1;
      for (size_t i = 0; i < fieldc; ++i) {
        value->fields[i] = DecodeValue(alloc, bits, prg, struct_decl->fieldv[i]);
      }
      return (Value*)value;
    }

    case UNION_DECL: {
      TypeDecl* union_decl = (TypeDecl*)decl;
      UnionValue* value = Alloc(alloc, sizeof(UnionValue));
      value->tag = ReadBits(bits, SizeOfTag(union_decl->fieldc));
      value->field = DecodeValue(alloc, bits, prg, union_decl->fieldv[value->tag]);
      return (Value*)value;
    }

    default:
      assert(false && "type id does not refer to a type declaration");
      return NULL;
  }
}

void EncodeValue(OutputBitStream* bits, Program* prg, Type type, Value* value)
{
  Decl* decl = prg->declv[type];
  switch (decl->tag) {
    case STRUCT_DECL: {
      TypeDecl* struct_decl = (TypeDecl*)decl;
      StructValue* struct_value = (StructValue*)value;
      size_t fieldc = struct_decl->fieldc;
      for (size_t i = 0; i < fieldc; ++i) {
        EncodeValue(bits, prg, struct_decl->fieldv[i], struct_value->fields[i]);
      }
      break;
    }

    case UNION_DECL: {
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
