
#ifndef INTERNAL_H_
#define INTERNAL_H_

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define GC_DEBUG
#include <gc/gc.h>
#define MALLOC_INIT() GC_INIT()
#define MALLOC(x) GC_MALLOC(x)
#define FREE(x) GC_FREE(x)
#define ENABLE_LEAK_DETECTION() GC_find_leak = 1
#define CHECK_FOR_LEAKS() GC_gcollect()

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
void* Alloc(Allocator* alloc, size_t size);
void FreeAll(Allocator* alloc);
void VectorInit(Allocator* alloc, Vector* vector, size_t size);
void* VectorAppend(Vector* vector);
void* VectorExtract(Vector* vector, size_t* count);

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
  ExprTag tag;
  DeclId func;
  size_t argc;
  Expr** argv;
} AppExpr;

typedef struct {
  ExprTag tag;
  DeclId type;
  Id field;
  Expr* body;
} UnionExpr;

typedef enum {
  STRUCT_DECL,    // TypeDecl
  UNION_DECL,     // TypeDecl
  FUNC_DECL,      // FuncDecl
  PROC_DECL       // ProcDecl
} DeclTag;

typedef struct {
  DeclTag tag;
} Decl;

typedef struct {
  DeclTag tag;
  size_t fieldc;
  Type* fieldv;
} TypeDecl;

typedef struct {
  DeclTag tag;
  size_t argc;
  Type* argv;
  Type return_type; 
  Expr* body;
} FuncDecl;

typedef struct {
  size_t declc;
  Decl** declv;
} Program;

// Value

#define UNION_KIND (-1)
typedef struct {
  size_t refcount;
  size_t kind;    // UNION_KIND for union, number of fields for struct.
} Value;

typedef struct {
  size_t refcount;
  size_t kind;
  Value* fields[];
} StructValue;

typedef struct {
  size_t refcount;
  size_t kind;
  size_t tag;
  Value* field;
} UnionValue;

StructValue* NewStructValue(size_t fieldc);
UnionValue* NewUnionValue();
Value* Copy(Value* src);
void Release(Value* value);

// BitStream
// Bit streams are represented as sequences of ascii digits '0' and '1'.
// TODO: Support more efficient encodings of bit streams when desired.
typedef struct {
  const char* bits;
} InputBitStream;

void OpenBinaryStringInputBitStream(InputBitStream* stream, const char* bits);
uint32_t ReadBits(InputBitStream* stream, size_t num_bits);

typedef struct {
  int fd;
} OutputBitStream;

void OpenBinaryOutputBitStream(OutputBitStream* stream, int fd);
void WriteBits(OutputBitStream* stream, size_t num_bits, uint32_t bits);

// Encoder
Value* DecodeValue(Allocator* alloc, InputBitStream* bits, Program* prg, Type type);
void EncodeValue(OutputBitStream* bits, Program* prg, Type type, Value* value);
Program* DecodeProgram(Allocator* alloc, InputBitStream* bits);

// Evaluator
Value* Execute(Program* program, FuncDecl* func);

#endif // INTERNAL_H_

