
typedef size_t DeclId;
typedef size_t Id;
typedef DeclId Type;

typedef enum {
  VAR_EXPR,
  APP_EXPR,
  UNION_EXPR,
  ACCESS_EXPR,
  COND_EXPR,
  LET_EXPR
} ExprTag;

typedef struct {
  ExprTag tag;
} Expr;

typedef struct {
  size_t exprc;
  Expr** exprs;
} Exprs;

typedef struct {
  size_t typec;
  Type* types;
} Types;

typedef struct {
  ExprTag tag;
  DeclId func;
  Exprs* args;
} AppExpr;

typedef enum { STRUCT_DECL, UNION_DECL, FUNC_DECL, PROC_DECL } DeclTag;

typedef struct {
  DeclTag tag;
} Decl;

typedef struct {
  DeclTag tag;
  Types types;
} StructDecl;

typedef struct {
  DeclTag tag;
  Types* args;
  Type return; 
  Expr* body;
} FuncDecl;

typedef struct {
  size_t declc;
  Decl** decls;
} Decls;

typedef Decls Program;

Id ParseId(Allocator* alloc, BitStream* bits) {
  switch (ReadBits(bits, 2)) {
    case 0: return 0;
    case 1: return 1;
    case 2: return 2 * ParseId(alloc, bits);
    case 3: return 2 * ParseId(alloc, bits) + 1;
    default: assert(false && "Unsupported Id tag");
  }
}

DeclId ParseDeclId(Allocator* alloc, BitStream* bits) {
  return ParseId(alloc, bits);
}

Types* ParseTypes(Allocator* alloc, BitStream* bits) {
  Vector vector;
  VectorInit(alloc, &vector, sizeof(Type));
  while (ReadBits(bits, 1)) {
    VectorAppendValue(&vector, ParseType(alloc, bits));
  }
  Types* types = Alloc(alloc, sizeof(Types));
  types->types = VectorExtract(&vector, &(types->typec));
  return types;
}

Type ParseType(Allocator* alloc, BitStream* bits) {
  return ParseId(alloc, bits);
}

Exprs* ParseExprs(Allocator* alloc, BitStream* bits) {
  Vector vector;
  VectorInit(alloc, &vector, sizeof(Expr*));
  while (ReadBits(bits, 1)) {
    VectorAppendValue(&vector, ParseExpr(alloc, bits));
  }
  Exprs* exprs = Alloc(alloc, sizeof(Exprs));
  exprs->exprs = VectorExtract(&vector, &(exprs->exprc));
  return exprs;
}

Expr* ParseExpr(Allocator* alloc, BitStream* bits) {
  switch (ReadBits(bits, 3)) {
    case APP_EXPR: {
      AppExpr* expr = Alloc(alloc, sizeof(AppExpr));
      expr->tag = APP_EXPR;
      expr->func = ParseDeclId(alloc, bits);
      expr->args = ParseExprs(alloc, bits);
      return (Expr*)expr;
    }

    default:
      assert(false && "TODO: Expr type");
      return NULL;
  }
}

Decl* ParseDecl(Allocator* alloc, BitStream* bits) {
  switch (ReadBits(bits, 2)) {
    case 0: {
      StructDecl* decl = Alloc(alloc, sizeof(StructDecl));
      decl->tag = STRUCT_DECL;
      ParseTypes(alloc, bits, &(decl->types));
      return (Decl*)decl;
    }

    case 1:
      assert(false && "TODO: Support UnionDecl tag");
      return NULL;

    case 2: {
      FuncDecl* decl = Alloc(alloc, sizeof(FuncDecl));
      decl->tag = FUNC_DECL;
      decl->args = ParseTypes(alloc, bits);
      decl->return = ParseType(alloc, bits);
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

Decls* ParseDecls(Allocator* alloc, BitStream* bits) {
  Vector vector;
  VectorInit(alloc, &vector, sizeof(Decl*));
  do {
    VectorAppendValue(&vector, ParseDecl(alloc, bits));
  } while (ReadBits(bits, 1));
  Decls* decls = Alloc(alloc, sizeof(Decls));
  decls->decls = VectorExtract(&vector, &(decls->declc));
  return decls;
}

Program* ParseProgram(Allocator* alloc, BitStream* bits) {
  return ParseDecls(alloc, bits);
}
