// FblcEvaluator.c --
//
//   This file implements routines for evaluating Fblc expressions.

#include "FblcInternal.h"

// An Fblc value is represented using the following FblcValue data structure.
// For struct values, 'tag' is unused and 'fieldv' contains the field data in
// the order the fields are declared in the type declaration.
// For union values, 'tag' is the index of the active field and 'fieldv' is a
// single element array with the field value.

struct FblcValue {
  FblcType* type;
  int tag;
  struct FblcValue* fieldv[];
};

// Singly-linked list of values.

typedef struct Values {
  FblcValue* value;
  struct Values* next;
} Values;

// A Link is a FIFO list of values. Values are put to the tail of the list and
// taken from the head of the list.
// The empty list is represented with head and tail both set to NULL.

typedef struct Link {
  Values* head;
  Values* tail;
} Link;

// The following defines a Vars structure for storing the value of local
// variables. It is possible to extend a local variable scope without
// modifying the original local variable scope, and it is possible to access
// the address of a value stored in the variable scope. Typical usage is to
// extend the variable scope by reserving slots for values that are yet to be
// computed, and to ensure the values are computed and updated by the time the
// scope is used.

typedef struct Vars {
  FblcName name;
  FblcValue* value;
  struct Vars* next;
} Vars;

// Map from port name to link.

typedef struct Ports {
  FblcName name;
  Link* link;
  struct Ports* next;
} Ports;

// The evaluator works by breaking down action and expression evaluation into
// a sequence of commands that can be executed in turn. All of the state of
// evaluation, including the stack, is stored explicitly in the command list.
// By storing the stack explicitly instead of piggy-backing off of the C
// runtime stack, we are able to implement the evaluator as a single while
// loop and avoid problems with supporting tail recursive Fblc programs.

// The following CmdTag enum identifies the types of commands used to evaluate
// actions and expressions. The Cmd struct represents a command list. See the
// comments on the extra data in the Cmd structure for the specification of
// the commands.
// The state of the stack is captured in a command list by storing variables
// in 'vars' commands.

typedef enum {
  CMD_EXPR, CMD_ACTN, CMD_ACCESS, CMD_COND, CMD_SCOPE, CMD_JOIN, CMD_PUT,
} CmdTag;

typedef struct Cmd {
  CmdTag tag;
  union {
    // The expr command evaluates expr and stores the resulting value in
    // *target.
    struct {
      const FblcExpr* expr;
      FblcValue** target;
    } expr;

    // The actn command executes actn and stores the resulting value, if any,
    // in *target.
    struct {
      FblcActn* actn;
      FblcValue** target;
    } actn;

    // The access command accesses the given field of the given value and
    // stores the resulting value in *target.
    struct {
      FblcValue* value;
      FblcName field;
      FblcValue** target;
    } access;

    // The cond command uses the tag of 'value' to select the choice to
    // evaluate. It then evaluates the chosen expression and stores the
    // resulting value in *target.
    struct {
      FblcValue* value;
      FblcExpr* const* choices;
      FblcValue** target;
    } cond;

    // The scope command sets the current ports and vars to the given ports
    // and vars.
    struct {
      Vars* vars;
      Ports* ports;
    } scope;

    // The join command halts the current thread of execution until count
    // has reached zero.
    struct {
      int count;
    } join;

    // The put command puts a value onto a link.
    struct {
      Link* link;
      FblcValue* value;
    } put;
  } ex;
  struct Cmd* next;
} Cmd;

// Singly-linked list of threads.

typedef struct Thread {
  Vars* vars;
  Ports* ports;
  Cmd* cmd;
  struct Thread* next;
} Thread;

// List of threads with head and tail pointer. Threads are added to the tail
// and removed from the head.

typedef struct {
  Thread* head;
  Thread* tail;
} Threads;

static FblcValue** LookupRef(Vars* vars, FblcName name);
static FblcValue* LookupVal(Vars* vars, FblcName name);
static Vars* AddVar(Vars* vars, FblcName name);

static Link* LookupPort(Ports* ports, FblcName name);
static Ports* AddPort(Ports* ports, FblcName name, Link* link);

static void AddThread(Threads* threads, Vars* vars, Ports* ports, Cmd* cmd);
static Thread* GetThread(Threads* threads);

static FblcValue* NewValue(FblcType* type);
static FblcValue* NewUnionValue(FblcType* type, int tag);
static Cmd* MkExpr(const FblcExpr* expr, FblcValue** target, Cmd* next);
static Cmd* MkActn(FblcActn* actn, FblcValue** target, Cmd* next);
static Cmd* MkAccess(
    FblcValue* value, FblcName field, FblcValue** target, Cmd* next);
static Cmd* MkCond(
    FblcValue* value, FblcExpr* const* choices, FblcValue** target, Cmd* next);
static Cmd* MkScope(Vars* vars, Ports* ports, Cmd* next);
static Cmd* MkJoin(int count, Cmd* next);
static int TagForField(const FblcType* type, FblcName field);
static void Run(const FblcEnv* env, Threads* threads, Vars* vars, Ports* ports,
    Cmd* cmd);

// LookupRef --
//
//   Look up a reference to the value of a variable in the given scope.
//
// Inputs:
//   vars - The scope to look in for the variable reference.
//   name - The name of the variable to lookup.
//
// Returns:
//   A pointer to the value of the variable with the given name in scope, or
//   NULL if no such variable or value was found in scope.
//
// Side effects:
//   None.

static FblcValue** LookupRef(Vars* vars, FblcName name)
{
  for ( ; vars != NULL; vars = vars->next) {
    if (FblcNamesEqual(vars->name, name)) {
      return &(vars->value);
    }
  }
  return NULL;
}

// LookupVal --
//
//   Look up the value of a variable in the given scope.
//
// Inputs:
//   vars - The scope to look in for the variable value.
//   name - The name of the variable to lookup.
//
// Returns:
//   The value of the variable with the given name in scope, or NULL if no
//   such variable or value was found in scope.
//
// Side effects:
//   None.

static FblcValue* LookupVal(Vars* vars, FblcName name)
{
  FblcValue** ref = LookupRef(vars, name);
  return ref == NULL ? NULL : *ref;
}

// AddVar --
//   
//   Extend the given scope with a new variable. Use LookupRef to access the
//   newly added variable.
//
// Inputs:
//   vars - The scope to extend.
//   name - The name of the variable to add.
//
// Returns:
//   A new scope with the new variable and the contents of the given scope.
//
// Side effects:
//   None.

static Vars* AddVar(Vars* vars, FblcName name)
{
  Vars* newvars = GC_MALLOC(sizeof(Vars));
  newvars->name = name;
  newvars->value = NULL;
  newvars->next = vars;
  return newvars;
}

// LookupPort --
//   
//   Lookup up the link associated with the given port.
//
// Inputs:
//   ports - The set of ports to look at.
//   name - The name of the port to look up.
//
// Returns:
//   The link associated with the given port, or NULL if no such port was
//   found.
//
// Side effects:
//   None.

static Link* LookupPort(Ports* ports, FblcName name)
{
  for ( ; ports != NULL; ports = ports->next) {
    if (FblcNamesEqual(ports->name, name)) {
      return ports->link;
    }
  }
  return NULL;
}

// AddPort --
//   
//   Extend the given port scope with a new port.
//
// Inputs:
//   ports - The scope to extend.
//   name - The name of the port to add.
//   link - The underlying link of the port to add.
//
// Returns:
//   A new scope with the new port and the contents of the given scope.
//
// Side effects:
//   None.

static Ports* AddPort(Ports* ports, FblcName name, Link* link)
{
  Ports* nports = GC_MALLOC(sizeof(Ports));
  nports->name = name;
  nports->link = link;
  nports->next = ports;
  return nports;
}

// AddThread --
//
//   Add a thread to the thread list.
//
// Inputs:
//   threads - The current thread list.
//   vars - The thread's local variables.
//   ports - The thread's ports.
//   cmd - The thread's command list.
//
// Results:
//   None.
//
// Side Effects:
//   A thread is added to the current thread list.

static void AddThread(Threads* threads, Vars* vars, Ports* ports, Cmd* cmd)
{
  Thread* thread = GC_MALLOC(sizeof(Thread));
  thread->vars = vars;
  thread->ports = ports;
  thread->cmd = cmd;
  thread->next = NULL;

  if (threads->head == NULL) {
    assert(threads->tail == NULL);
    threads->head = thread;
    threads->tail = thread;
  } else {
    threads->tail->next = thread;
  }
}

// GetThread --
//
//   Get the next thread from the thread list.
//
// Inputs:
//   threads - The current thread list.
//
// Results:
//   The next thread on the thread list, or NULL if there are no more threads
//   on the thread list.
//
// Side Effects:
//   The returned thread is removed from the current thread list.

static Thread* GetThread(Threads* threads)
{
  Thread* thread = threads->head;
  if (thread != NULL) {
    threads->head = thread->next;
    if (threads->head == NULL) {
      threads->tail = NULL;
    }
  }
  return thread;
}

// NewValue --
//
//   Create an uninitialized new value of the given type.
//
// Inputs:
//   type - The type of the value to create.
//
// Result:
//   The newly created value. The value with have the type field set, but the
//   fields will be uninitialized.
//
// Side effects:
//   None.

static FblcValue* NewValue(FblcType* type)
{
  int fieldc = type->fieldc;
  if (type->kind == FBLC_KIND_UNION) {
    fieldc = 1;
  }
  FblcValue* value = GC_MALLOC(sizeof(FblcValue) + fieldc * sizeof(FblcValue*));
  value->type = type;
  return value;
}

// NewUnionValue --
//
//   Create a new union value with the given type and tag.
//
// Inputs:
//   type - The type of the union value to create. This should be a union type.
//   tag - The tag of the union value to set.
//
// Result:
//   The new union value, with type and tag set. The field value will be
//   uninitialized.
//
// Side effects:
//   None.

static FblcValue* NewUnionValue(FblcType* type, int tag)
{
  assert(type->kind == FBLC_KIND_UNION);
  FblcValue* value = NewValue(type);
  value->tag = tag;
  return value;
}

// MkExpr --
//
//   Creates a command to evaluate the given expression, storing the resulting
//   value at the given target location.
//
// Inputs:
//   expr - The expression for the created command to evaluate. This must not
//          be NULL.
//   target - The target of the result of evaluation. This must not be NULL.
//   next - The command to run after this command.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkExpr(const FblcExpr* expr, FblcValue** target, Cmd* next)
{
  assert(expr != NULL);
  assert(target != NULL);

  Cmd* cmd = GC_MALLOC(sizeof(Cmd));
  cmd->tag = CMD_EXPR;
  cmd->ex.expr.expr = expr;
  cmd->ex.expr.target = target;
  cmd->next = next;
  return cmd;
}

// MkActn --
//
//   Creates a command to evaluate the given action, storing the resulting
//   value, if any, at the given target location.
//
// Inputs:
//   actn - The action for the created command to evaluate. This must not
//          be NULL.
//   target - The target of the result of evaluation.
//   next - The command to run after this command.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkActn(FblcActn* actn, FblcValue** target, Cmd* next)
{
  assert(actn != NULL);

  Cmd* cmd = GC_MALLOC(sizeof(Cmd));
  cmd->tag = CMD_ACTN;
  cmd->ex.actn.actn = actn;
  cmd->ex.actn.target = target;
  cmd->next = next;
  return cmd;
}

// MkAccess --
//   
//   Create a command to access a field from a value, storing the result at
//   the given target location.
//
// Inputs:
//   value - The value that will be accessed.
//   field - The name of the field to access.
//   target - The target destination of the accessed field.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkAccess(
    FblcValue* value, FblcName field, FblcValue** target, Cmd* next)
{
  Cmd* cmd = GC_MALLOC(sizeof(Cmd));
  cmd->tag = CMD_ACCESS;
  cmd->ex.access.value = value;
  cmd->ex.access.field = field;
  cmd->ex.access.target = target;
  cmd->next = next;
  return cmd;
}

// MkCond --
//   
//   Create a command to select and evaluate an expression based on the tag of
//   the given value.
//
// Inputs:
//   value - The value to condition on.
//   choices - The list of expressions to choose from based on the value.
//   target - The target destination for the final evaluated value.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkCond(
    FblcValue* value, FblcExpr* const* choices, FblcValue** target, Cmd* next)
{
  Cmd* cmd = GC_MALLOC(sizeof(Cmd));
  cmd->tag = CMD_COND;
  cmd->ex.cond.value = value;
  cmd->ex.cond.choices = choices;
  cmd->ex.cond.target = target;
  cmd->next = next;
  return cmd;
}

// MkScope --
//   
//   Create a command to change the current ports and local variable scope.
//
// Inputs:
//   vars - The new value of the local variable scope to use.
//   ports - The new value of the ports scope to use.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkScope(Vars* vars, Ports* ports, Cmd* next)
{
  Cmd* cmd = GC_MALLOC(sizeof(Cmd));
  cmd->tag = CMD_SCOPE;
  cmd->ex.scope.vars = vars;
  cmd->ex.scope.ports = ports;
  cmd->next = next;
  return cmd;
}

// MkJoin --
//
//   Create a join command.
//
// Inputs:
//   count - The number of threads to join.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkJoin(int count, Cmd* next)
{
  Cmd* cmd = GC_MALLOC(sizeof(Cmd));
  cmd->tag = CMD_JOIN;
  cmd->ex.join.count = count;
  cmd->next = next;
  return cmd;
}

// MkPut --
//
//   Create a put command.
//
// Inputs:
//   link - The link to put the value on.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkPut(Link* link, Cmd* next)
{
  Cmd* cmd = GC_MALLOC(sizeof(Cmd));
  cmd->tag = CMD_PUT;
  cmd->ex.put.link = link;
  cmd->ex.put.value = NULL;
  cmd->next = next;
  return cmd;
}

// TagForField --
//
//   Return the tag corresponding to the named field for the given type.
//   It is a program error if the named field does not exist for the given
//   type.
//
// Inputs:
//   type - The type in question.
//   field - The field to get the tag for.
//
// Result:
//   The index in the list of fields for the type containing that field name.
//
// Side effects:
//   None.

static int TagForField(const FblcType* type, FblcName field)
{
  for (int i = 0; i < type->fieldc; i++) {
    if (FblcNamesEqual(field, type->fieldv[i].name.name)) {
      return i;
    }
  }
  assert(false && "No such field.");
}

// Run --
//
//   Spend a finite amount of time executing commands for a thread.
//
// Inputs:
//   env - The environment in which to run the thread.
//   threads - The list of currently active threads.
//   vars - Local variables in scope.
//   ports - Ports in scope.
//   cmd - The head of the command list for the thread.
//
// Result:
//   None.
//
// Side Effects:
//   Threads are added to the threads list based on the command executed. If
//   the command list is not executed to completion, a thread will be added to
//   the threads list representing the continuation of this thread.

static void Run(const FblcEnv* env, Threads* threads, Vars* vars, Ports* ports,
    Cmd* cmd)
{
  for (int i = 0; i < 1024 && cmd != NULL; i++) {
    switch (cmd->tag) {
      case CMD_EXPR: {
        const FblcExpr* expr = cmd->ex.expr.expr;
        FblcValue** target = cmd->ex.expr.target;
        cmd = cmd->next;
        switch (expr->tag) {
          case FBLC_VAR_EXPR: {
            FblcName var_name = expr->ex.var.name.name;
            *target = LookupVal(vars, var_name);
            assert(*target != NULL && "Var not in scope");
            break;
          }

          case FBLC_APP_EXPR: {
            FblcType* type = FblcLookupType(env, expr->ex.app.func.name);
            if (type != NULL) {
              if (type->kind == FBLC_KIND_STRUCT) {
                // Create the struct value now, then add commands to evaluate
                // the arguments to fill in the fields with the proper results.
                *target = NewValue(type);
                for (int i = 0; i < type->fieldc; i++) {
                  cmd = MkExpr(expr->argv[i], &((*target)->fieldv[i]), cmd);
                }
              } else {
                assert(false && "Invalid kind of type for application");
              }
              break;
            }

            FblcFunc* func = FblcLookupFunc(env, expr->ex.app.func.name);
            if (func != NULL) {
              // Add to the top of the command list:
              // arg -> ... -> arg -> scope -> body -> (scope) -> ...
              // The results of the arg evaluations will be stored directly in
              // the vars for the vars command.

              // Don't put a scope change if we will immediately 
              // change it back to a different scope. This is important to
              // avoid memory leaks for tail calls.
              if (cmd != NULL && cmd->tag != CMD_SCOPE) {
                cmd = MkScope(vars, ports, cmd);
              }

              cmd = MkExpr(func->body, target, cmd);
              cmd = MkScope(NULL, ports, cmd);

              Cmd* scmd = cmd;
              Vars* nvars = NULL;
              for (int i = 0; i < func->argc; i++) {
                FblcName var_name = func->argv[i].name.name;
                nvars = AddVar(nvars, var_name);
                cmd = MkExpr(expr->argv[i], LookupRef(nvars, var_name), cmd);
              }
              scmd->ex.scope.vars = nvars;
              break;
            }
            assert(false && "No such struct type or function found");
          }

          case FBLC_ACCESS_EXPR: {
            // Add to the top of the command list:
            // object -> access -> ...
            cmd = MkAccess(NULL, expr->ex.access.field.name, target, cmd);
            cmd = MkExpr(expr->ex.access.object, &(cmd->ex.access.value), cmd);
            break;
          }

          case FBLC_UNION_EXPR: {
            // Create the union value now, then add a command to evaluate the
            // argument of the union constructor and set the field of the
            // union value.
            FblcType* type = FblcLookupType(env, expr->ex.union_.type.name);
            assert(type != NULL);
            int tag = TagForField(type, expr->ex.union_.field.name);
            *target = NewUnionValue(type, tag);
            cmd = MkExpr(expr->ex.union_.value, &((*target)->fieldv[0]), cmd);
            break;
          }

          case FBLC_LET_EXPR: {
            // Add to the top of the command list:
            // def -> var -> body -> (vars) -> ...

            // No need to reset the scope if we are going to switch to
            // different scope immediately after anyway.
            if (cmd != NULL && cmd->tag != CMD_SCOPE) {
              cmd = MkScope(vars, ports, cmd);
            }
            FblcName var_name = expr->ex.let.name.name;
            Vars* nvars = AddVar(vars, var_name);
            cmd = MkExpr(expr->ex.let.body, target, cmd);
            cmd = MkScope(nvars, ports, cmd);
            cmd = MkExpr(expr->ex.let.def, LookupRef(nvars, var_name), cmd);
            break;
          }

          case FBLC_COND_EXPR: {
            // Add to the top of the command list:
            // select -> cond -> ...
            cmd = MkCond(NULL, expr->argv, target, cmd);
            cmd = MkExpr(expr->ex.cond.select, &(cmd->ex.cond.value), cmd);
            break;
          }
        }
        break;
      }

      case CMD_ACTN: {
        FblcActn* actn = cmd->ex.actn.actn;
        FblcValue** target = cmd->ex.actn.target;
        cmd = cmd->next;
        switch (actn->tag) {
          case FBLC_EVAL_ACTN: {
            cmd = MkExpr(actn->ac.eval.expr, target, cmd);
            break;
          }

          case FBLC_GET_ACTN: {
            Link* link = LookupPort(ports, actn->ac.get.port.name);
            assert(link != NULL && "Get port not in scope");
            assert(link->head != NULL && "TODO: Allow get to block");
            *target = link->head->value;
            link->head = link->head->next;
            if (link->head == NULL) {
              link->tail = NULL;
            }
            break;
          }

          case FBLC_PUT_ACTN: {
            // expr -> put -> next
            Link* link = LookupPort(ports, actn->ac.put.port.name);
            assert(link != NULL && "Put port not in scope");
            cmd = MkPut(link, cmd);
            cmd = MkExpr(actn->ac.put.expr, &(cmd->ex.put.value), cmd);
            break;
          }

          case FBLC_CALL_ACTN:
            // expr -> expr -> ...
            //      -> ports -> vars -> body -> ports -> vars -> next
            assert(false && "TODO: Exec CALL_ACTN");
            break;

          case FBLC_LINK_ACTN: {
            Link* link = GC_MALLOC(sizeof(Link));
            link->head = NULL;
            link->tail = NULL;
            ports = AddPort(ports, actn->ac.link.getname.name, link);
            ports = AddPort(ports, actn->ac.link.putname.name, link);
            cmd = MkActn(actn->ac.link.body, target, cmd);
            break;
          }

          case FBLC_EXEC_ACTN: {
            // Create new threads for each action to execute.
            // actn .>
            // actn ..> join -> vars -> actn -> vars -> next
            // actn .>
            cmd = MkScope(vars, ports, cmd);
            cmd = MkActn(actn->ac.exec.body, target, cmd);
            Cmd* scmd = MkScope(NULL, ports, cmd);
            Cmd* jcmd = MkJoin(actn->ac.exec.execc, scmd);

            Vars* nvars = vars;
            for (int i = 0; i < actn->ac.exec.execc; i++) {
              FblcExec* exec = &(actn->ac.exec.execv[i]);
              FblcValue** target = NULL;
              if (exec->var != NULL) {
                nvars = AddVar(vars, exec->var->name.name);
                target = LookupRef(vars, exec->var->name.name);
              }
              AddThread(threads, vars, ports, MkActn(exec->actn, target, jcmd));
            }
            scmd->ex.scope.vars = nvars;
            cmd = NULL;
            break;
          }

          case FBLC_COND_ACTN:
            assert(false && "TODO: Exec COND_ACTN");
            break;
        }
        break;
      }               

      case CMD_ACCESS: {
        FblcType* type = cmd->ex.access.value->type;
        int target_tag = TagForField(type, cmd->ex.access.field);
        int actual_tag = cmd->ex.access.value->tag;
        FblcValue** target = cmd->ex.access.target;
        if (type->kind == FBLC_KIND_STRUCT) {
          *target = cmd->ex.access.value->fieldv[target_tag];
        } else if (actual_tag == target_tag) {
          *target = cmd->ex.access.value->fieldv[0];
        } else {
          fprintf(stderr, "MEMBER ACCESS UNDEFINED\n");
          abort();
        }
        cmd = cmd->next;
        break;
      }

      case CMD_COND: {
        FblcValue* value = cmd->ex.cond.value;
        FblcValue** target = cmd->ex.cond.target;
        assert(value->type->kind == FBLC_KIND_UNION);
        cmd = MkExpr(cmd->ex.cond.choices[value->tag], target, cmd->next);
        break;
      }

      case CMD_SCOPE:
        vars = cmd->ex.scope.vars;
        ports = cmd->ex.scope.ports;
        cmd = cmd->next;
        break;

      case CMD_JOIN:
        assert(cmd->ex.join.count > 0);
        cmd->ex.join.count--;
        cmd = cmd->ex.join.count == 0 ? cmd->next : NULL;
        break;

      case CMD_PUT: {
        Link* link = cmd->ex.put.link;
        assert(cmd->ex.put.value != NULL);
        Values* ntail = GC_MALLOC(sizeof(Values));
        ntail->value = cmd->ex.put.value;
        ntail->next = NULL;
        if (link->head == NULL) {
          assert(link->tail == NULL);
          link->head = ntail;
        } else {
          link->tail->next = ntail;
        }
        link->tail = ntail;
        cmd = cmd->next;
        break;
      }
    }
  }

  if (cmd != NULL) {
    AddThread(threads, vars, ports, cmd);
  }
}

// FblcPrintValue --
//
//   Print a value in standard format to the given FILE stream.
//
// Inputs:
//   stream - The stream to print the value to.
//   value - The value to print.
//
// Result:
//   None.
//
// Side effects:
//   The value is printed to the given file stream.

void FblcPrintValue(FILE* stream, FblcValue* value)
{
  FblcType* type = value->type;
  if (type->kind == FBLC_KIND_STRUCT) {
    fprintf(stream, "%s(", type->name.name);
    for (int i = 0; i < type->fieldc; i++) {
      if (i > 0) {
        fprintf(stream, ",");
      }
      FblcPrintValue(stream, value->fieldv[i]);
    }
    fprintf(stream, ")");
  } else if (type->kind == FBLC_KIND_UNION) {
    fprintf(stream, "%s:%s(",
        type->name.name, type->fieldv[value->tag].name.name);
    FblcPrintValue(stream, value->fieldv[0]);
    fprintf(stream, ")");
  } else {
    assert(false && "Invalid Kind");
  }
}

// FblcEvaluate --
//
//   Evaluate an expression under the given program environment. The program
//   and expression must be well formed.
//
// Inputs:
//   env - The program environment.
//   expr - The expression to evaluate.
//
// Returns:
//   The result of evaluating the given expression in the program environment
//   in a scope with no local variables.
//
// Side effects:
//   None.

FblcValue* FblcEvaluate(const FblcEnv* env, const FblcExpr* expr)
{
  FblcActn* actn = GC_MALLOC(sizeof(FblcActn));
  actn->tag = FBLC_EVAL_ACTN;
  actn->loc = expr->loc;
  actn->ac.eval.expr = expr;
  return FblcExecute(env, actn);
}

// FblcExecute --
//
//   Execute an action under the given program environment. The program
//   and action must be well formed.
//
// Inputs:
//   env - The program environment.
//   actn - The action to execute.
//
// Returns:
//   The result of executing the given action in the program environment
//   in a scope with no local variables and no ports.
//
// Side effects:
//   None.

FblcValue* FblcExecute(const FblcEnv* env, FblcActn* actn)
{
  FblcValue* result = NULL;
  Threads threads;
  threads.head = NULL;
  threads.tail = NULL;

  AddThread(&threads, NULL, NULL, MkActn(actn, &result, NULL));

  Thread* thread = GetThread(&threads);
  while (thread != NULL) {
    Run(env, &threads, thread->vars, thread->ports, thread->cmd);
    thread = GetThread(&threads);
  }
  return result;
}
