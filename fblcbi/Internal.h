
#ifndef INTERNAL_H_
#define INTERNAL_H_

// Allocator
typedef struct AllocList AllocList;

typedef struct {
  AllocList* allocations;
} Allocator;

// Vector is a helper for dynamically allocating an array of data whose
// size is not known ahead of time.
// 'size' is the number of bytes taken by a single element.
// 'capacity' is the maximum number of elements supported by the current
// allocation of data.
// 'count' is the number of elements currently in use.
// 'data' is where the data is stored.

typedef struct {
  Allocator* allocator;
  int size;
  int capacity;
  int count;
  void* data;
} Vector ;

void InitAllocator(Allocator* alloc);
void* Alloc(Allocator* alloc, int size);
void FreeAll(Allocator* alloc);
void VectorInit(Allocator* alloc, Vector* vector, int size);
void* VectorAppend(Vector* vector);
void* VectorExtract(Vector* vector, int* count);

// Program
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

typedef enum {
  STRUCT_DECL,
  UNION_DECL,
  FUNC_DECL,
  PROC_DECL
} DeclTag;

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

// Value
typedef struct {
  size_t refcount;
} Value;

typedef struct {
  size_t refcount;
  Value* fields[];
} StructValue;

typedef struct {
  size_t refcount;
  size_t tag;
  Value field;
} UnionValue;

// BitStream
uint32_t ReadBits(BitStream* stream, int num_bits);

// Parser
Program* ParseProgram(Allocator* alloc, BitStream* bits)

#endif // INTERNAL_H_

