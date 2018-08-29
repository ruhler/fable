// eval.c --
//   This file implements the fble evaluation routines.

#include <stdio.h>    // for fprintf, stderr
#include <string.h>   // for memcpy

#include "fble.h"

#define UNREACHABLE(x) assert(false && x)

// Vars --
//   Scope of variables visible during type checking.
typedef struct Vars {
  FbleName name;
  FbleType* type;
  struct Vars* next;
} Vars;

// VStack --
//   A stack of values.
typedef struct VStack {
  FbleValue* value;
  struct VStack* tail;
} VStack;

// InstrTag --
//   Enum used to distinguish among different kinds of instructions.
typedef enum {
  STRUCT_VALUE_INSTR,
  STRUCT_ACCESS_INSTR,
  UNION_VALUE_INSTR,
  UNION_ACCESS_INSTR,
  COND_INSTR,
  FUNC_VALUE_INSTR,
  FUNC_APPLY_INSTR,
  VAR_INSTR,
  LET_INSTR,
  PUSH_INSTR,
  POP_INSTR,
} InstrTag;

// Instr --
//   Common base type for all instructions.
typedef struct {
  InstrTag tag;
} Instr;

// InstrV --
//   A vector of Instr.
typedef struct {
  size_t size;
  Instr** xs;
} InstrV;

// VarInstr -- VAR_INSTR
//   Reads the variable at the given position in the stack. Position 0 is the
//   top of the stack.
typedef struct {
  Instr _base;
  size_t position;
} VarInstr;

// PopInstr -- POP_INSTR
//   Pop count values from the value stack.
typedef struct {
  Instr _base;
  size_t count;
} PopInstr;

// LetInstr -- LET_INSTR
//   Evaluate each of the bindings, add the results to the scope, then execute
//   the body.
typedef struct {
  Instr _base;
  InstrV bindings;
  Instr* body;
  PopInstr pop;
} LetInstr;

// FbleFuncValue -- FBLE_FUNC_VALUE
//
// Fields:
//   context - The value stack at the time the function was created,
//             representing the lexical context available to the function.
//             Stored in reverse order of the standard value stack.
//   body - The instr representing the body of the function.
//          Note: fbleFuncValue does not take ownership of body. The
//          FuncValueInstr that allocates the FbleFuncValue has ownership of
//          the body.
//   pop - An instruction that can be used to pop the arguments, the
//         context, and the function value itself after a function is done
//         executing.
struct FbleFuncValue {
  FbleValue _base;
  VStack* context;
  Instr* body;
  PopInstr pop;
};

// FuncValueInstr -- FUNC_VALUE_INSTR
//   Allocate a function, capturing the current variable context in the
//   process.
typedef struct {
  Instr _base;
  size_t argc;
  Instr* body;
} FuncValueInstr;

// FuncApplyInstr -- FUNC_APPLY_INSTR
//   Given f, x1, x2, ... on the top of the value stack,
//   apply f(x1, x2, ...).
// From the top of the stack down, we should find the arguments in reverse
// order and then the function value.
typedef struct {
  Instr _base;
  size_t argc;
} FuncApplyInstr;

// StructValueInstr -- STRUCT_VALUE_INSTR
//   Allocate a struct value, then execute each of its arguments.
typedef struct {
  Instr _base;
  InstrV fields;
} StructValueInstr;

// AccessInstr -- STRUCT_ACCESS_INSTR or UNION_ACCESS_INSTR
//   Access the tagged field from the object on top of the vstack.
typedef struct {
  Instr _base;
  FbleLoc loc;
  size_t tag;
} AccessInstr;

// PushInstr -- PUSH_INSTR
//   Evaluate and push the given values on top of the value stack and execute
//   the following instruction.
typedef struct {
  Instr _base;
  InstrV values;
  Instr* next;
} PushInstr;

// UnionValueInstr -- UNION_VALUE_INSTR
//   Allocate a union value, then execute its argument.
typedef struct {
  Instr _base;
  size_t tag;
  Instr* mkarg;
} UnionValueInstr;

// CondInstr -- COND_INSTR
//   Select the next thing to execute based on the tag of the value on top of
//   the value stack.
typedef struct {
  Instr _base;
  InstrV choices;
} CondInstr;

// ThreadStack --
//   The computation context for a thread.
// 
// Fields:
//   result - Pointer to where the result of evaluating an expression should
//            be stored.
//   instr -  The instructions for executing the expression.
typedef struct ThreadStack {
  FbleValue** result;
  Instr* instr;
  struct ThreadStack* tail;
} ThreadStack;

static bool TypesEqual(FbleType* a, FbleType* b);
static void PrintType(FbleType* type);

static VStack* VPush(FbleArena* arena, FbleValue* value, VStack* tail);
static VStack* VPop(FbleArena* arena, VStack* vstack);
static ThreadStack* TPush(FbleArena* arena, FbleValue** presult, Instr* instr, ThreadStack* tail); 

static FbleType* Compile(FbleArena* arena, Vars* vars, Vars* type_vars, FbleExpr* expr, Instr** instrs);
static FbleType* CompileType(FbleArena* arena, Vars* vars, FbleType* type);
static FbleValue* Eval(FbleArena* arena, Instr* instrs, VStack* stack);
static void FreeInstrs(FbleArena* arena, Instr* instrs);

// TypesEqual --
//   Test whether the two given compiled types are equal.
//
// Inputs:
//   a - the first type
//   b - the second type
//
// Results:
//   True if the first type equals the second type, false otherwise.
//
// Side effects:
//   None.
static bool TypesEqual(FbleType* a, FbleType* b)
{
  if (a == b) {
    return true;
  }

  if (a->tag != b->tag) {
    return false;
  }

  switch (a->tag) {
    case FBLE_STRUCT_TYPE: {
      FbleStructType* sta = (FbleStructType*)a;
      FbleStructType* stb = (FbleStructType*)b;
      if (sta->fields.size != stb->fields.size) {
        return false;
      }

      for (size_t i = 0; i < sta->fields.size; ++i) {
        if (!FbleNamesEqual(sta->fields.xs[i].name.name, stb->fields.xs[i].name.name)) {
          return false;
        }
        
        if (!TypesEqual(sta->fields.xs[i].type, stb->fields.xs[i].type)) {
          return false;
        }
      }
      return true;
    }

    case FBLE_UNION_TYPE: {
      FbleUnionType* uta = (FbleUnionType*)a;
      FbleUnionType* utb = (FbleUnionType*)b;
      if (uta->fields.size != utb->fields.size) {
        return false;
      }

      for (size_t i = 0; i < uta->fields.size; ++i) {
        if (!FbleNamesEqual(uta->fields.xs[i].name.name, utb->fields.xs[i].name.name)) {
          return false;
        }
        
        if (!TypesEqual(uta->fields.xs[i].type, utb->fields.xs[i].type)) {
          return false;
        }
      }
      return true;
    }

    case FBLE_FUNC_TYPE: {
      FbleFuncType* fta = (FbleFuncType*)a;
      FbleFuncType* ftb = (FbleFuncType*)b;
      if (fta->args.size != ftb->args.size) {
        return false;
      }

      for (size_t i = 0; i < fta->args.size; ++i) {
        if (!FbleNamesEqual(fta->args.xs[i].name.name, ftb->args.xs[i].name.name)) {
          return false;
        }
        
        if (!TypesEqual(fta->args.xs[i].type, ftb->args.xs[i].type)) {
          return false;
        }
      }
      return TypesEqual(fta->rtype, ftb->rtype);
    }

    case FBLE_PROC_TYPE: assert(false && "TODO PROC_TYPE"); return false;
    case FBLE_INPUT_TYPE: assert(false && "TODO INPUT_TYPE"); return false;
    case FBLE_OUTPUT_TYPE: assert(false && "TODO OUTPUT_TYPE"); return false;
    case FBLE_VAR_TYPE: UNREACHABLE("Uncompiled var type for eq"); return false;
    case FBLE_LET_TYPE: UNREACHABLE("Uncompiled let type for eq"); return false;
    case FBLE_POLY_TYPE: assert(false && "TODO POLY_TYPE"); return false;
    case FBLE_POLY_APPLY_TYPE: assert(false && "TODO POLY_APPLY_TYPE"); return false;
  }

  UNREACHABLE("should never get here");
}

// PrintType --
//   Print the given compiled type in human readable form to stderr.
//
// Inputs:
//   type - the type to print.
//
// Result:
//   None.
//
// Side effect:
//   Prints the given type in human readable form to stderr.
static void PrintType(FbleType* type)
{
  switch (type->tag) {
    case FBLE_STRUCT_TYPE: {
      FbleStructType* stv = (FbleStructType*)type;
      fprintf(stderr, "*(");
      const char* comma = "";
      for (size_t i = 0; i < stv->fields.size; ++i) {
        fprintf(stderr, "%s", comma);
        PrintType(stv->fields.xs[i].type);
        fprintf(stderr, " %s", stv->fields.xs[i].name.name);
        comma = ", ";
      }
      fprintf(stderr, ")");
      return;
    };

    case FBLE_UNION_TYPE: {
      FbleUnionType* utv = (FbleUnionType*)type;
      fprintf(stderr, "+(");
      const char* comma = "";
      for (size_t i = 0; i < utv->fields.size; ++i) {
        fprintf(stderr, "%s", comma);
        PrintType(utv->fields.xs[i].type);
        fprintf(stderr, " %s", utv->fields.xs[i].name.name);
        comma = ", ";
      }
      fprintf(stderr, ")");
      return;
    };

    case FBLE_FUNC_TYPE: {
      FbleFuncType* ftv = (FbleFuncType*)type;
      fprintf(stderr, "\\(");
      const char* comma = "";
      for (size_t i = 0; i < ftv->args.size; ++i) {
        fprintf(stderr, "%s", comma);
        PrintType(ftv->args.xs[i].type);
        fprintf(stderr, " %s", ftv->args.xs[i].name.name);
        comma = ", ";
      }
      fprintf(stderr, "; ");
      PrintType(ftv->rtype);
      fprintf(stderr, ")");
      return;
    };

    case FBLE_PROC_TYPE: assert(false && "TODO PROC_TYPE"); return;
    case FBLE_INPUT_TYPE: assert(false && "TODO INPUT_TYPE"); return;
    case FBLE_OUTPUT_TYPE: assert(false && "TODO OUTPUT_TYPE"); return;
    case FBLE_VAR_TYPE: UNREACHABLE("Uncompiled var type"); return;
    case FBLE_LET_TYPE: UNREACHABLE("Uncompiled let type"); return;
    case FBLE_POLY_TYPE: assert(false && "TODO POLY_TYPE"); return;
    case FBLE_POLY_APPLY_TYPE: assert(false && "TODO POLY_APPLY_TYPE"); return;
  }
}

// TPush --
//   Push a value onto a value stack.
//
// Inputs:
//   arena - the arena to use for allocations
//   value - the value to push
//   tail - the stack to push to
//
// Result:
//   The new stack with pushed value.
//
// Side effects:
//   Allocates a new VStack instance that should be freed when done.
static VStack* VPush(FbleArena* arena, FbleValue* value, VStack* tail)
{
  VStack* vstack = FbleAlloc(arena, VStack);
  vstack->value = value;
  vstack->tail = tail;
  return vstack;
}

// VPop --
//   Pop a value off the value stack.
//
// Inputs:
//   arena - the arena to use for deallocation
//   stack - the stack to pop from
//
// Results:
//   The popped stack.
//
// Side effects:
//   Frees the top stack frame. It is the users job to release the value if
//   necessary before popping the top of the stack.
static VStack* VPop(FbleArena* arena, VStack* vstack)
{
  VStack* tail = vstack->tail;
  FbleFree(arena, vstack);
  return tail;
}

// TPush --
//   Push an element onto a thread stack.
//
// Inputs:
//   arena - the arena to use for allocations
//   presult - the result pointer to push
//   instr - the instr to push
//   tail - the stack to push to
//
// Result:
//   The new stack with pushed value.
//
// Side effects:
//   Allocates a new ThreadStack instance that should be freed when done.
static ThreadStack* TPush(FbleArena* arena, FbleValue** presult, Instr* instr, ThreadStack* tail)
{
  assert(instr != NULL && "TPush NULL Instr");
  ThreadStack* tstack = FbleAlloc(arena, ThreadStack);
  tstack->result = presult;
  tstack->instr = instr;
  tstack->tail = tail;
  return tstack;
}

// Compile --
//   Type check and compile the given expression.
//
// Inputs:
//   arena - arena to use for allocations.
//   vars - the list of variables in scope.
//   type_vars - the value of type variables in scope.
//   expr - the expression to compile.
//   instrs - output pointer to store generated in structions
//
// Results:
//   The type of the expression, or NULL if the expression is not well typed.
//
// Side effects:
//   Sets instrs to point to the compiled instructions if the expression is
//   well typed. Prints a message to stderr if the expression fails to compile.
static FbleType* Compile(FbleArena* arena, Vars* vars, Vars* type_vars, FbleExpr* expr, Instr** instrs)
{
  switch (expr->tag) {
    case FBLE_VAR_EXPR: {
      FbleVarExpr* var_expr = (FbleVarExpr*)expr;

      int position = 0;
      while (vars != NULL && !FbleNamesEqual(var_expr->var.name, vars->name.name)) {
        vars = vars->next;
        position++;
      }

      if (vars == NULL) {
        FbleReportError("variable '%s' not defined\n", &var_expr->var.loc, var_expr->var.name);
        return NULL;
      }

      VarInstr* instr = FbleAlloc(arena, VarInstr);
      instr->_base.tag = VAR_INSTR;
      instr->position = position;
      *instrs = &instr->_base;
      return vars->type;
    }
//
//    case FBLE_LET_EXPR: {
//      FbleLetExpr* let_expr = (FbleLetExpr*)expr;
//      bool error = false;
//
//      // Evaluate the types of the bindings and set up the new vars.
//      Vars nvarsd[let_expr->bindings.size];
//      VStack nvstackd[let_expr->bindings.size];
//      Vars* nvars = vars;
//      VStack* nvstack = vstack;
//      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
//        nvarsd[i].name = let_expr->bindings.xs[i].name;
//        nvarsd[i].type = CompileType(arena, vars, vstack, let_expr->bindings.xs[i].type);
//        error = error || (nvarsd[i].type == NULL);
//        nvarsd[i].next = nvars;
//        nvars = nvarsd + i;
//
//        // TODO: Push an abstract value here instead of NULL?
//        nvstackd[i].value = NULL;
//        nvstackd[i].tail = nvstack;
//        nvstack = nvstackd + i;
//      }
//
//      // Compile, check, and if appropriate, evaluate the values of the
//      // variables.
//      LetInstr* instr = FbleAlloc(arena, LetInstr);
//      instr->_base.tag = LET_INSTR;
//      FbleVectorInit(arena, instr->bindings);
//      instr->body = NULL;
//      instr->pop._base.tag = POP_INSTR;
//      instr->pop.count = let_expr->bindings.size;
//
//      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
//        Instr* mkval = NULL;
//        FbleValue* type = NULL;
//        if (!error) {
//          type = Compile(arena, nvars, nvstack, let_expr->bindings.xs[i].expr, &mkval);
//        }
//        error = error || (type == NULL);
//        FbleVectorAppend(arena, instr->bindings, mkval);
//
//        if (!error && !TypesEqual(nvarsd[i].type, type)) {
//          error = true;
//          FbleReportError("expected type ", &let_expr->bindings.xs[i].expr->loc);
//          PrintType(nvarsd[i].type);
//          fprintf(stderr, ", but found ");
//          PrintType(type);
//          fprintf(stderr, "\n");
//        }
//        FbleRelease(arena, type);
//
//        if (!error && IsKinded(nvarsd[i].type)) {
//          assert(nvstackd[i].value == NULL);
//          nvstackd[i].value = Eval(arena, mkval, nvstack);
//          error = (nvstackd[i].value == NULL);
//        }
//      }
//
//      FbleValue* result = NULL;
//      if (!error) {
//        result = Compile(arena, nvars, nvstack, let_expr->body, &(instr->body));
//        error = (result == NULL);
//      }
//
//      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
//        FbleRelease(arena, nvarsd[i].type);
//        FbleRelease(arena, nvstackd[i].value);
//      }
//
//      if (error) {
//        FreeInstrs(arena, &instr->_base);
//        FbleRelease(arena, result);
//        return NULL;
//      }
//
//      *instrs = &instr->_base;
//      return result;
//    }
//
//
//      FbleValue* rtypetype = Compile(arena, vars, vstack, func_type_expr->rtype, &instr->rtype);
//      error = error || (rtypetype == NULL);
//
//      if (rtypetype != NULL && !TypesEqual(rtypetype, &gTypeTypeValue)) {
//        FbleReportError("expected a type, but found something of type ", &func_type_expr->rtype->loc);
//        PrintType(rtypetype);
//        fprintf(stderr, "\n");
//        error = true;
//      }
//      FbleRelease(arena, rtypetype);
//
//      if (error) {
//        FreeInstrs(arena, &instr->_base);
//        return NULL;
//      }
//
//      *instrs = &instr->_base;
//      return FbleCopy(arena, &gTypeTypeValue);
//    }
//
//    case FBLE_FUNC_VALUE_EXPR: {
//      FbleFuncValueExpr* func_value_expr = (FbleFuncValueExpr*)expr;
//      FbleFuncTypeValue* func_type = FbleAlloc(arena, FbleFuncTypeValue);
//      func_type->_base.tag = FBLE_FUNC_TYPE_VALUE;
//      func_type->_base.refcount = 1;
//      FbleVectorInit(arena, func_type->fields);
//      func_type->rtype = NULL;
//
//      Vars nvarsd[func_value_expr->args.size];
//      VStack nvstackd[func_value_expr->args.size];
//      Vars* nvars = vars;
//      VStack* nvstack = vstack;
//      bool error = false;
//      for (size_t i = 0; i < func_value_expr->args.size; ++i) {
//        FbleFieldValue* field = FbleVectorExtend(arena, func_type->fields);
//        field->name = func_value_expr->args.xs[i].name;
//        field->type = CompileType(arena, vars, vstack, func_value_expr->args.xs[i].type);
//        error = error || (field->type == NULL);
//
//        for (size_t j = 0; j < i; ++j) {
//          if (FbleNamesEqual(field->name.name, func_value_expr->args.xs[j].name.name)) {
//            error = true;
//            FbleReportError("duplicate arg name '%s'\n", &field->name.loc, field->name.name);
//            break;
//          }
//        }
//
//        nvarsd[i].name = field->name;
//        nvarsd[i].type = field->type;
//        nvarsd[i].next = nvars;
//        nvars = nvarsd + i;
//
//        // TODO: Push an abstract value here instead of NULL?
//        nvstackd[i].value = NULL;
//        nvstackd[i].tail = nvstack;
//        nvstack = nvstackd + i;
//      }
//
//      FuncValueInstr* instr = FbleAlloc(arena, FuncValueInstr);
//      instr->_base.tag = FUNC_VALUE_INSTR;
//      instr->argc = func_value_expr->args.size;
//      instr->body = NULL;
//      if (!error) {
//        func_type->rtype = Compile(arena, nvars, nvstack, func_value_expr->body, &instr->body);
//        error = error || (func_type->rtype == NULL);
//      }
//
//      if (error) {
//        FbleRelease(arena, &func_type->_base);
//        FreeInstrs(arena, &instr->_base);
//        return NULL;
//      }
//
//      *instrs = &instr->_base;
//      return &func_type->_base;
//    }
//
    case FBLE_STRUCT_VALUE_EXPR: {
      FbleStructValueExpr* struct_value_expr = (FbleStructValueExpr*)expr;
      FbleType* type = CompileType(arena, type_vars, struct_value_expr->type);
      if (type == NULL) {
        return NULL;
      }

      if (type->tag != FBLE_STRUCT_TYPE) {
        FbleReportError("expected a struct type, but found ", &struct_value_expr->type->loc);
        PrintType(type);
        fprintf(stderr, "\n");
        return NULL;
      }

      FbleStructType* struct_type = (FbleStructType*)type;
      if (struct_type->fields.size != struct_value_expr->args.size) {
        // TODO: Where should the error message go?
        FbleReportError("expected %i args, but %i were provided\n",
            &expr->loc, struct_type->fields.size, struct_value_expr->args.size);
        return NULL;
      }

      bool error = false;
      StructValueInstr* instr = FbleAlloc(arena, StructValueInstr);
      instr->_base.tag = STRUCT_VALUE_INSTR;
      FbleVectorInit(arena, instr->fields);
      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        FbleField* field = struct_type->fields.xs + i;

        Instr* mkarg = NULL;
        FbleType* arg_type = Compile(arena, vars, type_vars, struct_value_expr->args.xs[i], &mkarg);
        error = error || (arg_type == NULL);

        if (arg_type != NULL && !TypesEqual(field->type, arg_type)) {
          FbleReportError("expected type ", &struct_value_expr->args.xs[i]->loc);
          PrintType(field->type);
          fprintf(stderr, ", but found ");
          PrintType(arg_type);
          fprintf(stderr, "\n");
          error = true;
        }

        FbleVectorAppend(arena, instr->fields, mkarg);
      }

      if (error) {
        FreeInstrs(arena, &instr->_base);
        return NULL;
      }

      *instrs = &instr->_base;
      return type;
    }
//
//
//    case FBLE_UNION_VALUE_EXPR: {
//      FbleUnionValueExpr* union_value_expr = (FbleUnionValueExpr*)expr;
//      FbleValue* type = CompileType(arena, vars, vstack, union_value_expr->type);
//      if (type == NULL) {
//        return NULL;
//      }
//
//      if (type->tag != FBLE_UNION_TYPE_VALUE) {
//        FbleReportError("expected a union type, but found ", &union_value_expr->type->loc);
//        PrintType(type);
//        fprintf(stderr, "\n");
//        FbleRelease(arena, type);
//        return NULL;
//      }
//
//      bool error = false;
//      FbleUnionTypeValue* union_type = (FbleUnionTypeValue*)type;
//      FbleValue* field_type = NULL;
//      size_t tag = 0;
//      for (size_t i = 0; i < union_type->fields.size; ++i) {
//        FbleFieldValue* field = union_type->fields.xs + i;
//        if (FbleNamesEqual(field->name.name, union_value_expr->field.name)) {
//          tag = i;
//          field_type = field->type;
//          break;
//        }
//      }
//
//      if (field_type == NULL) {
//        FbleReportError("'%s' is not a field of type ", &union_value_expr->field.loc, union_value_expr->field.name);
//        PrintType(type);
//        fprintf(stderr, "\n");
//        error = true;
//      }
//
//      Instr* mkarg = NULL;
//      FbleValue* arg_type = Compile(arena, vars, vstack, union_value_expr->arg, &mkarg);
//      if (arg_type == NULL) {
//        error = true;
//      }
//
//      if (field_type != NULL && arg_type != NULL && !TypesEqual(field_type, arg_type)) {
//        FbleReportError("expected type ", &union_value_expr->arg->loc);
//        PrintType(field_type);
//        fprintf(stderr, ", but found type ");
//        PrintType(arg_type);
//        fprintf(stderr, "\n");
//        error = true;
//      }
//      FbleRelease(arena, arg_type);
//
//      if (error) {
//        FreeInstrs(arena, mkarg);
//        FbleRelease(arena, type);
//        return NULL;
//      }
//
//      UnionValueInstr* instr = FbleAlloc(arena, UnionValueInstr);
//      instr->_base.tag = UNION_VALUE_INSTR;
//      instr->tag = tag;
//      instr->mkarg = mkarg;
//      *instrs = &instr->_base;
//      return type;
//    }
//
//    case FBLE_ACCESS_EXPR: {
//      FbleAccessExpr* access_expr = (FbleAccessExpr*)expr;
//
//      // Allocate a slot on the variable stack for the intermediate value
//      Vars nvars = {
//        .name = { .name = "", .loc = expr->loc },
//        .type = NULL,
//        .next = vars,
//      };
//
//      // TODO: Push an abstract value here instead of NULL?
//      // Or should we try to compute the actual value?
//      VStack nvstack = { .value = NULL, .tail = vstack };
//
//      PushInstr* instr = FbleAlloc(arena, PushInstr);
//      instr->_base.tag = PUSH_INSTR;
//      FbleVectorInit(arena, instr->values);
//      Instr** mkobj = FbleVectorExtend(arena, instr->values);
//      *mkobj = NULL;
//
//      AccessInstr* access = FbleAlloc(arena, AccessInstr);
//      access->_base.tag = STRUCT_ACCESS_INSTR;
//      instr->next = &access->_base;
//      access->loc = access_expr->field.loc;
//      FbleValue* type = Compile(arena, &nvars, &nvstack, access_expr->object, mkobj);
//
//      FbleFieldValueV* fields = NULL;
//      if (type != NULL && type->tag == FBLE_STRUCT_TYPE_VALUE) {
//        access->_base.tag = STRUCT_ACCESS_INSTR;
//        fields = &((FbleStructTypeValue*)type)->fields;
//      } else if (type != NULL && type->tag == FBLE_UNION_TYPE_VALUE) {
//        access->_base.tag = UNION_ACCESS_INSTR;
//        fields = &((FbleUnionTypeValue*)type)->fields;
//      } else {
//        if (type != NULL) {
//          FbleReportError("expected value of type struct or union, but found value of type ", &access_expr->object->loc);
//          PrintType(type);
//          fprintf(stderr, "\n");
//          FbleRelease(arena, type);
//        }
//
//        FreeInstrs(arena, &instr->_base);
//        return NULL;
//      }
//
//      for (size_t i = 0; i < fields->size; ++i) {
//        if (FbleNamesEqual(access_expr->field.name, fields->xs[i].name.name)) {
//          access->tag = i;
//          *instrs = &instr->_base;
//          FbleValue* field_type = FbleCopy(arena, fields->xs[i].type);
//          FbleRelease(arena, type);
//          return field_type;
//        }
//      }
//
//      FbleReportError("%s is not a field of type ", &access_expr->field.loc, access_expr->field.name);
//      PrintType(type);
//      fprintf(stderr, "\n");
//      FreeInstrs(arena, &instr->_base);
//      FbleRelease(arena, type);
//      return NULL;
//    }
//
//    case FBLE_COND_EXPR: {
//      FbleCondExpr* cond_expr = (FbleCondExpr*)expr;
//
//      // Allocate a slot on the variable stack for the intermediate value
//      Vars nvars = {
//        .name = { .name = "", .loc = expr->loc },
//        .type = NULL,
//        .next = vars,
//      };
//
//      // TODO: Push an abstract value here instead of NULL?
//      // Or should we try to compute the actual value?
//      VStack nvstack = { .value = NULL, .tail = vstack };
//
//      PushInstr* push = FbleAlloc(arena, PushInstr);
//      push->_base.tag = PUSH_INSTR;
//      FbleVectorInit(arena, push->values);
//      push->next = NULL;
//
//      Instr** mkobj = FbleVectorExtend(arena, push->values);
//      *mkobj = NULL;
//      FbleValue* type = Compile(arena, &nvars, &nvstack, cond_expr->condition, mkobj);
//      if (type == NULL) {
//        FreeInstrs(arena, &push->_base);
//        return NULL;
//      }
//
//      if (type->tag != FBLE_UNION_TYPE_VALUE) {
//        FbleReportError("expected value of union type, but found value of type ", &cond_expr->condition->loc);
//        PrintType(type);
//        fprintf(stderr, "\n");
//        FbleRelease(arena, type);
//        FreeInstrs(arena, &push->_base);
//        return NULL;
//      }
//
//      FbleUnionTypeValue* union_type = (FbleUnionTypeValue*)type;
//      if (union_type->fields.size != cond_expr->choices.size) {
//        FbleReportError("expected %d arguments, but %d were provided.\n", &cond_expr->_base.loc, union_type->fields.size, cond_expr->choices.size);
//        FbleRelease(arena, type);
//        FreeInstrs(arena, &push->_base);
//        return NULL;
//      }
//
//      CondInstr* cond_instr = FbleAlloc(arena, CondInstr);
//      cond_instr->_base.tag = COND_INSTR;
//      push->next = &cond_instr->_base;
//      FbleVectorInit(arena, cond_instr->choices);
//
//      FbleValue* return_type = NULL;
//      for (size_t i = 0; i < cond_expr->choices.size; ++i) {
//        if (!FbleNamesEqual(cond_expr->choices.xs[i].name.name, union_type->fields.xs[i].name.name)) {
//          FbleReportError("expected tag '%s', but found '%s'.\n",
//              &cond_expr->choices.xs[i].name.loc,
//              union_type->fields.xs[i].name.name,
//              cond_expr->choices.xs[i].name.name);
//          FbleRelease(arena, return_type);
//          FbleRelease(arena, type);
//          FreeInstrs(arena, &push->_base);
//          return NULL;
//        }
//
//        Instr* mkarg = NULL;
//        FbleValue* arg_type = Compile(arena, vars, vstack, cond_expr->choices.xs[i].expr, &mkarg);
//        if (arg_type == NULL) {
//          FbleRelease(arena, return_type);
//          FbleRelease(arena, type);
//          FreeInstrs(arena, &push->_base);
//          return NULL;
//        }
//        FbleVectorAppend(arena, cond_instr->choices, mkarg);
//
//        if (return_type == NULL) {
//          return_type = arg_type;
//        } else {
//          if (!TypesEqual(return_type, arg_type)) {
//            FbleReportError("expected type ", &cond_expr->choices.xs[i].expr->loc);
//            PrintType(return_type);
//            fprintf(stderr, ", but found ");
//            PrintType(arg_type);
//            fprintf(stderr, "\n");
//
//            FbleRelease(arena, arg_type);
//            FbleRelease(arena, return_type);
//            FbleRelease(arena, type);
//            FreeInstrs(arena, &push->_base);
//            return NULL;
//          }
//          FbleRelease(arena, arg_type);
//        }
//      }
//
//      FbleRelease(arena, type);
//      *instrs = &push->_base;
//      return return_type;
//    }
//
//    case FBLE_EVAL_EXPR: assert(false && "TODO: FBLE_EVAL_EXPR"); return NULL;
//    case FBLE_LINK_EXPR: assert(false && "TODO: FBLE_LINK_EXPR"); return NULL;
//    case FBLE_EXEC_EXPR: assert(false && "TODO: FBLE_EXEC_EXPR"); return NULL;
//
//    case FBLE_APPLY_EXPR: {
//      FbleApplyExpr* apply_expr = (FbleApplyExpr*)expr;
//
//      // Allocate space on the stack for the intermediate values.
//      Vars nvarsd[1 + apply_expr->args.size];
//      VStack nvstackd[1 + apply_expr->args.size];
//      Vars* nvars = vars;
//      VStack* nvstack = vstack;
//      for (size_t i = 0; i < 1 + apply_expr->args.size; ++i) {
//        nvarsd[i].type = NULL;
//        nvarsd[i].name.name = "";
//        nvarsd[i].name.loc = expr->loc;
//        nvarsd[i].next = nvars;
//        nvars = nvarsd + i;
//
//        // TODO: Push an abstract value here instead of NULL?
//        nvstackd[i].value = NULL;
//        nvstackd[i].tail = nvstack;
//        nvstack = nvstackd + i;
//      };
//
//      PushInstr* push = FbleAlloc(arena, PushInstr);
//      push->_base.tag = PUSH_INSTR;
//      FbleVectorInit(arena, push->values);
//      push->next = NULL;
//
//      Instr** mkfunc = FbleVectorExtend(arena, push->values);
//      *mkfunc = NULL;
//      FbleValue* type = Compile(arena, nvars, nvstack, apply_expr->func, mkfunc);
//      if (type == NULL) {
//        FreeInstrs(arena, &push->_base);
//        return NULL;
//      }
//
//      switch (type->tag) {
//        case FBLE_FUNC_TYPE_VALUE: {
//          FbleFuncTypeValue* func_type = (FbleFuncTypeValue*)type;
//          bool error = false;
//          if (func_type->fields.size != apply_expr->args.size) {
//            FbleReportError("expected %i args, but %i provided\n",
//                &expr->loc, func_type->fields.size, apply_expr->args.size);
//            error = true;
//          }
//
//          for (size_t i = 0; i < func_type->fields.size && i < apply_expr->args.size; ++i) {
//            Instr** mkarg = FbleVectorExtend(arena, push->values);
//            *mkarg = NULL;
//            FbleValue* arg_type = Compile(arena, nvars, nvstack, apply_expr->args.xs[i], mkarg);
//            error = error || (arg_type == NULL);
//            if (arg_type != NULL && !TypesEqual(func_type->fields.xs[i].type, arg_type)) {
//              FbleReportError("expected type ", &apply_expr->args.xs[i]->loc);
//              PrintType(func_type->fields.xs[i].type);
//              fprintf(stderr, ", but found ");
//              PrintType(arg_type);
//              fprintf(stderr, "\n");
//              error = true;
//            }
//            FbleRelease(arena, arg_type);
//          }
//
//          if (error) {
//            FbleRelease(arena, type);
//            FreeInstrs(arena, &push->_base);
//            return NULL;
//          }
//
//          FuncApplyInstr* apply_instr = FbleAlloc(arena, FuncApplyInstr);
//          apply_instr->_base.tag = FUNC_APPLY_INSTR;
//          apply_instr->argc = func_type->fields.size;
//          push->next = &apply_instr->_base;
//          *instrs = &push->_base;
//          FbleValue* rtype = FbleCopy(arena, func_type->rtype);
//          FbleRelease(arena, type);
//          return rtype;
//        }
//
//        default: {
//          FbleReportError("cannot perform application on an object of type ", &expr->loc);
//          PrintType(type);
//          FbleRelease(arena, type);
//          FreeInstrs(arena, &push->_base);
//          return NULL;
//        }
//      }
//
//      UNREACHABLE("should not get here");
//      return NULL;
//    }

    default:
      UNREACHABLE("invalid expression tag");
      return NULL;
  }

  UNREACHABLE("should already have returned");
  return NULL;
}

// CompileType --
//   Compile and evaluate a type.
//
// Inputs:
//   arena - arena to use for allocations.
//   vars - the value of type variables in scope.
//   type - the type to compile.
//
// Results:
//   The compiled and evaluated type, or NULL in case of error.
//
// Side effects:
//   Prints a message to stderr if the type fails to compile or evalute.
static FbleType* CompileType(FbleArena* arena, Vars* vars, FbleType* type)
{
  switch (type->tag) {
    case FBLE_STRUCT_TYPE: {
      FbleStructType* struct_type = (FbleStructType*)type;
      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        FbleField* field = struct_type->fields.xs + i;
        FbleType* compiled = CompileType(arena, vars, field->type);
        if (compiled == NULL) {
          return NULL;
        }
        field->type = compiled;

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(field->name.name, struct_type->fields.xs[j].name.name)) {
            FbleReportError("duplicate field name '%s'\n", &field->name.loc, field->name.name);
            return NULL;
          }
        }
      }
      return &struct_type->_base;
    }

    case FBLE_UNION_TYPE: {
      FbleUnionType* union_type = (FbleUnionType*)type;
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        FbleField* field = union_type->fields.xs + i;
        FbleType* compiled = CompileType(arena, vars, field->type);
        if (compiled == NULL) {
          return NULL;
        }
        field->type = compiled;

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(field->name.name, union_type->fields.xs[j].name.name)) {
            FbleReportError("duplicate field name '%s'\n", &field->name.loc, &field->name.name);
            return NULL;
          }
        }
      }
      return &union_type->_base;
    }

    case FBLE_FUNC_TYPE: {
      FbleFuncType* func_type = (FbleFuncType*)type;
      for (size_t i = 0; i < func_type->args.size; ++i) {
        FbleField* field = func_type->args.xs + i;

        FbleType* compiled = CompileType(arena, vars, field->type);
        if (compiled == NULL) {
          return NULL;
        }
        field->type = compiled;

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(field->name.name, func_type->args.xs[j].name.name)) {
            FbleReportError("duplicate arg name '%s'\n", &field->name.loc, field->name.name);
            return NULL;
          }
        }
      }

      FbleType* compiled = CompileType(arena, vars, func_type->rtype);
      if (compiled == NULL) {
        return NULL;
      }
      func_type->rtype = compiled;
      return &func_type->_base;
    }

    case FBLE_PROC_TYPE: assert(false && "TODO: FBLE_PROC_TYPE"); return NULL;
    case FBLE_INPUT_TYPE: assert(false && "TODO: FBLE_INPUT_TYPE"); return NULL;
    case FBLE_OUTPUT_TYPE: assert(false && "TODO: FBLE_OUTPUT_TYPE"); return NULL;

    case FBLE_VAR_TYPE: {
      FbleVarType* var_type = (FbleVarType*)type;

      while (vars != NULL && !FbleNamesEqual(var_type->var.name, vars->name.name)) {
        vars = vars->next;
      }

      if (vars == NULL) {
        FbleReportError("variable '%s' not defined\n", &var_type->var.loc, var_type->var.name);
        return NULL;
      }
      return vars->type;
    }

    default:
      UNREACHABLE("invalid type tag");
      return NULL;
  }

  UNREACHABLE("should already have returned");
  return NULL;
}

// Eval --
//   Execute the given sequence of instructions to completion.
//
// Inputs:
//   arena - the arena to use for allocations.
//   instrs - the instructions to evaluate.
//   vstack - the stack of values in scope.
//
// Results:
//   The computed value, or NULL on error.
//
// Side effects:
//   Prints a message to stderr in case of error.
static FbleValue* Eval(FbleArena* arena, Instr* prgm, VStack* vstack_in)
{
  VStack* vstack = vstack_in;
  FbleValue* final_result = NULL;
  ThreadStack* tstack = TPush(arena, &final_result, prgm, NULL);

  while (tstack != NULL) {
    FbleValue** presult = tstack->result;
    Instr* instr = tstack->instr;
    ThreadStack* tstack_done = tstack;
    tstack = tstack->tail;
    FbleFree(arena, tstack_done);

    switch (instr->tag) {
      case VAR_INSTR: {
        VarInstr* var_instr = (VarInstr*)instr;
        VStack* v = vstack;
        for (size_t i = 0; i < var_instr->position; ++i) {
          assert(v->tail != NULL);
          v = v->tail;
        }
        *presult = FbleCopy(arena, v->value);
        break;
      }

      case LET_INSTR: {
        LetInstr* let_instr = (LetInstr*)instr;

        tstack = TPush(arena, NULL, &let_instr->pop._base, tstack);
        tstack = TPush(arena, presult, let_instr->body, tstack);

        for (size_t i = 0; i < let_instr->bindings.size; ++i) {
          vstack = VPush(arena, NULL, vstack);
          tstack = TPush(arena, &vstack->value, let_instr->bindings.xs[i], tstack);
        }
        break;
      }

      case FUNC_VALUE_INSTR: {
        FuncValueInstr* func_value_instr = (FuncValueInstr*)instr;
        FbleFuncValue* value = FbleAlloc(arena, FbleFuncValue);
        value->_base.tag = FBLE_FUNC_VALUE;
        value->_base.refcount = 1;
        value->context = NULL;
        value->body = func_value_instr->body;
        value->pop._base.tag = POP_INSTR;
        value->pop.count = 1 + func_value_instr->argc;

        for (VStack* vs = vstack; vs != NULL;  vs = vs->tail) {
          value->context = VPush(arena, FbleCopy(arena, vs->value), value->context);
          value->pop.count++;
        }
        
        // Set the result after copying the context, to avoid introducing a
        // circular reference chain from this value to itself.
        *presult = &value->_base;
        break;
      }

      case FUNC_APPLY_INSTR: {
        FuncApplyInstr* apply_instr = (FuncApplyInstr*)instr;
        FbleValue* args[apply_instr->argc];
        for (size_t i = 0; i < apply_instr->argc; ++i) {
          args[i] = vstack->value;
          vstack = VPop(arena, vstack);
        }

        FbleFuncValue* func = (FbleFuncValue*)vstack->value;
        assert(func->_base.tag = FBLE_FUNC_VALUE);

        // Push the function's context on top of the value stack.
        for (VStack* vs = func->context; vs != NULL; vs = vs->tail) {
          vstack = VPush(arena, FbleCopy(arena, vs->value), vstack);
        }

        // Push the function args on top of the value stack.
        for (size_t i = 0; i < apply_instr->argc; ++i) {
          size_t j = apply_instr->argc - 1 - i;
          vstack = VPush(arena, args[j], vstack);
        }

        tstack = TPush(arena, NULL, &func->pop._base, tstack);
        tstack = TPush(arena, presult, func->body, tstack);
        break;
      }

      case STRUCT_VALUE_INSTR: {
        StructValueInstr* struct_value_instr = (StructValueInstr*)instr;
        FbleStructValue* value = FbleAlloc(arena, FbleStructValue);
        value->_base.tag = FBLE_STRUCT_VALUE;
        value->_base.refcount = 1;
        value->fields.size = struct_value_instr->fields.size;
        value->fields.xs = FbleArenaAlloc(arena, value->fields.size * sizeof(FbleValue*), FbleAllocMsg(__FILE__, __LINE__));
        *presult = &value->_base;

        for (size_t i = 0; i < struct_value_instr->fields.size; ++i) {
          tstack = TPush(arena, value->fields.xs + i, struct_value_instr->fields.xs[i], tstack);
        }
        break;
      }

      case STRUCT_ACCESS_INSTR: {
        AccessInstr* access_instr = (AccessInstr*)instr;
        assert(vstack != NULL);
        FbleStructValue* value = (FbleStructValue*)vstack->value;
        assert(value->_base.tag == FBLE_STRUCT_VALUE);

        assert(access_instr->tag < value->fields.size);
        *presult = FbleCopy(arena, value->fields.xs[access_instr->tag]);

        FbleRelease(arena, vstack->value);
        vstack = VPop(arena, vstack);
        break;
      }

      case UNION_VALUE_INSTR: {
        UnionValueInstr* union_value_instr = (UnionValueInstr*)instr;
        FbleUnionValue* union_value = FbleAlloc(arena, FbleUnionValue);
        union_value->_base.tag = FBLE_UNION_VALUE;
        union_value->_base.refcount = 1;
        union_value->tag = union_value_instr->tag;
        union_value->arg = NULL;

        *presult = &union_value->_base;

        tstack = TPush(arena, &union_value->arg, union_value_instr->mkarg, tstack);
        break;
      }

      case UNION_ACCESS_INSTR: {
        AccessInstr* access_instr = (AccessInstr*)instr;

        assert(vstack != NULL);
        FbleUnionValue* value = (FbleUnionValue*)vstack->value;
        assert(value->_base.tag == FBLE_UNION_VALUE);

        if (value->tag != access_instr->tag) {
          FbleReportError("union field access undefined: wrong tag\n", &access_instr->loc);

          // Clean up the stacks.
          while (vstack != vstack_in) {
            FbleRelease(arena, vstack->value);
            vstack = VPop(arena, vstack);
          }

          while (tstack != NULL) {
            ThreadStack* otstack = tstack;
            tstack = tstack->tail;
            FbleFree(arena, otstack);
          }

          return NULL;
        }
        *presult = FbleCopy(arena, value->arg);

        FbleRelease(arena, vstack->value);
        vstack = VPop(arena, vstack);
        break;
      }

      case COND_INSTR: {
        CondInstr* cond_instr = (CondInstr*)instr;
        assert(vstack != NULL);
        FbleUnionValue* value = (FbleUnionValue*)vstack->value;
        assert(value->_base.tag == FBLE_UNION_VALUE);

        assert(value->tag < cond_instr->choices.size);
        tstack = TPush(arena, presult, cond_instr->choices.xs[value->tag], tstack);

        FbleRelease(arena, vstack->value);
        vstack = VPop(arena, vstack);
        break;
      }

      case PUSH_INSTR: {
        PushInstr* push_instr = (PushInstr*)instr;

        tstack = TPush(arena, presult, push_instr->next, tstack);
        for (size_t i = 0; i < push_instr->values.size; ++i) {
          vstack = VPush(arena, NULL, vstack);
          tstack = TPush(arena, &vstack->value, push_instr->values.xs[i], tstack);
        }
        break;
      }

      case POP_INSTR: {
        PopInstr* pop_instr = (PopInstr*)instr;
        for (size_t i = 0; i < pop_instr->count; ++i) {
          assert(vstack != NULL);
          FbleRelease(arena, vstack->value);
          vstack = VPop(arena, vstack);
        }
        break;
      }
    }
  }
  assert(vstack == vstack_in);
  return final_result;
}

// FreeInstrs --
//   Free the given sequence of instructions.
//
// Inputs:
//   arena - the arena used to allocation the instructions.
//   instrs - the instructions to free. May be NULL.
//
// Result:
//   none.
//
// Side effect:
//   Frees memory allocated for the instrs.
static void FreeInstrs(FbleArena* arena, Instr* instrs)
{
  if (instrs == NULL) {
    return;
  }

  switch (instrs->tag) {
    case VAR_INSTR:
    case FUNC_APPLY_INSTR:
    case STRUCT_ACCESS_INSTR:
    case UNION_ACCESS_INSTR:
    case POP_INSTR: {
      FbleFree(arena, instrs);
      return;
    }

    case LET_INSTR: {
      LetInstr* let_instr = (LetInstr*)instrs;
      for (size_t i = 0; i < let_instr->bindings.size; ++i) {
        FreeInstrs(arena, let_instr->bindings.xs[i]);
      }
      FbleFree(arena, let_instr->bindings.xs);
      FreeInstrs(arena, let_instr->body);
      FbleFree(arena, let_instr);
      return;
    }

    case FUNC_VALUE_INSTR: {
      FuncValueInstr* func_value_instr = (FuncValueInstr*)instrs;
      FreeInstrs(arena, func_value_instr->body);
      FbleFree(arena, func_value_instr);
      return;
    }

    case STRUCT_VALUE_INSTR: {
      StructValueInstr* instr = (StructValueInstr*)instrs;
      for (size_t i = 0; i < instr->fields.size; ++i) {
        FreeInstrs(arena, instr->fields.xs[i]);
      }
      FbleFree(arena, instr->fields.xs);
      FbleFree(arena, instr);
      return;
    }

    case UNION_VALUE_INSTR: {
      UnionValueInstr* instr = (UnionValueInstr*)instrs;
      FreeInstrs(arena, instr->mkarg);
      FbleFree(arena, instrs);
      return;
    }

    case COND_INSTR: {
      CondInstr* instr = (CondInstr*)instrs;
      for (size_t i = 0; i < instr->choices.size; ++i) {
        FreeInstrs(arena, instr->choices.xs[i]);
      }
      FbleFree(arena, instr->choices.xs);
      FbleFree(arena, instrs);
      return;
    }

    case PUSH_INSTR: {
      PushInstr* push_instr = (PushInstr*)instrs;
      for (size_t i = 0; i < push_instr->values.size; ++i) {
        FreeInstrs(arena, push_instr->values.xs[i]);
      }
      FbleFree(arena, push_instr->values.xs);
      FreeInstrs(arena, push_instr->next);
      FbleFree(arena, instrs);
      return;
    }
  }
  UNREACHABLE("invalid instruction");
}

// FbleEval -- see documentation in fble.h
FbleValue* FbleEval(FbleArena* arena, FbleExpr* expr)
{
  Instr* instrs = NULL;
  FbleType* type = Compile(arena, NULL, NULL, expr, &instrs);
  if (type == NULL) {
    return NULL;
  }

  VStack* vstack = NULL;
  FbleValue* result = Eval(arena, instrs, vstack);
  FreeInstrs(arena, instrs);
  return result;
}

// FbleFreeFuncValue -- see documentation in fble.h
void FbleFreeFuncValue(FbleArena* arena, FbleFuncValue* value)
{
  assert(false && "TODO: FbleFreeFuncValue");
  VStack* vs = value->context;
  while (vs != NULL) {
    FbleRelease(arena, vs->value);
    vs = VPop(arena, vs);
  }

  // Note: The FbleFuncValue does not take ownership of value->body, so we
  // should not free it here.
  FbleFree(arena, value);
}
