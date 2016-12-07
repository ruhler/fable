
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
  ExprTag tag;
  DeclId func;
  Exprs* args;
} AppExpr;

typedef struct Exprs {
  Expr* head;
  struct Exprs* tail;
} Exprs;

typedef struct Types {
  Type head;
  struct Types* tail;
} Types;

typedef enum { STRUCT_DECL, UNION_DECL, FUNC_DECL, PROC_DECL } DeclTag;

typedef struct {
  DeclTag tag;
} Decl;

typedef struct {
  DeclTag tag;
  Types* types;
} StructDecl;

typedef struct {
  DeclTag tag;
  Types* args;
  Type return; 
  Expr* body;
} FuncDecl;

typedef struct Decls {
  Decl* head;
  struct Decls* tail;
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
  Types* head = NULL;
  Types** tail = &head;
  while (ReadBits(bits, 1)) {
    Types* types = Alloc(alloc, sizeof(Types));
    types->head = ParseType(alloc, bits);
    *tail = types;
    tail = &(types->tail);
  }
  return head;
}

Type ParseType(Allocator* alloc, BitStream* bits) {
  return ParseId(alloc, bits);
}

Exprs* ParseExprs(Allocator* alloc, BitStream* bits) {
  Exprs* head = NULL;
  Exprs** tail = &head;
  while (ReadBits(bits, 1)) {
    Exprs* exprs = Alloc(alloc, sizeof(Exprs));
    exprs->head = ParseExpr(alloc, bits);
    *tail = exprs;
    tail = &(exprs->tail);
  }
  return head;
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
      decl->types = ParseTypes(alloc, bits);
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
  Decls* head = NULL;
  Decls** tail = &head;
  do {
    Decls* decls = Alloc(alloc, sizeof(Decls));
    decls->head = ParseDecl(alloc, bits);
    *tail = decls;
    tail = &(decls->tail);
  } while (ReadBits(bits, 1));
  return head;
}

Program* ParseProgram(Allocator* alloc, BitStream* bits) {
  return ParseDecls(alloc, bits);
}
