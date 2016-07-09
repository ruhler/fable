// FblcEvaluator.c --
//
//   This file implements routines for evaluating Fblc expressions.

#include "FblcInternal.h"

// Singly-linked list of values.

typedef struct Values {
  FblcValue* value;
  struct Values* next;
} Values;

// A Link is a FIFO list of values. Values are put to the tail of the list and
// taken from the head of the list.
// The empty list is represented with head and tail both set to NULL.
// waiting is a list of threads waiting to get values from the link.

typedef struct Link {
  Values* head;
  Values* tail;
  struct Thread* waiting;
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
  CMD_EXPR, CMD_ACTN, CMD_ACCESS, CMD_COND_EXPR, CMD_COND_ACTN,
  CMD_SCOPE, CMD_JOIN, CMD_PUT,
} CmdTag;

// Cmd is the base structure common to all commands.
// Specific commands will have the same initial layout, with additional
// data following.
typedef struct Cmd {
  CmdTag tag;
  struct Cmd* next;
} Cmd;

// CMD_EXPR: The expr command evaluates expr and stores the resulting value in
// *target.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  const FblcExpr* expr;
  FblcValue** target;
} ExprCmd;

// CMD_ACTN: The actn command executes actn and stores the resulting value, if
// any, in *target.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  FblcActn* actn;
  FblcValue** target;
} ActnCmd;

// CMD_ACCESS: The access command accesses the given field of the given value
// and stores the resulting value in *target.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  FblcValue* value;
  FblcName field;
  FblcValue** target;
} AccessCmd;

// CMD_COND_EXPR: The condition expression command uses the tag of 'value' to
// select the choice to evaluate. It then evaluates the chosen expression and
// stores the resulting value in *target.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  FblcValue* value;
  FblcExpr* const* choices;
  FblcValue** target;
} CondExprCmd;

// CMD_COND_ACTN: The conditional action command uses the tag of 'value' to
// select the choice to evaluate. It then evaluates the chosen action and
// stores the resulting value in *target.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  FblcValue* value;
  FblcActn** choices;
  FblcValue** target;
} CondActnCmd;

// CMD_SCOPE: The scope command sets the current ports and vars to the given
// ports and vars.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  Vars* vars;
  Ports* ports;
} ScopeCmd;

// CMD_JOIN: The join command halts the current thread of execution until
// count has reached zero.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  int count;
} JoinCmd;

// CMD_PUT: The put command puts a value onto a link and into the target.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  FblcValue** target;
  Link* link;
  FblcValue* value;
} PutCmd;

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

static Cmd* MkExprCmd(const FblcExpr* expr, FblcValue** target, Cmd* next);
static Cmd* MkActnCmd(FblcActn* actn, FblcValue** target, Cmd* next);
static Cmd* MkAccessCmd(
    FblcValue* value, FblcName field, FblcValue** target, Cmd* next);
static Cmd* MkCondExprCmd(
    FblcValue* value, FblcExpr* const* choices, FblcValue** target, Cmd* next);
static Cmd* MkCondActnCmd(
    FblcValue* value, FblcActn** choices, FblcValue** target, Cmd* next);
static Cmd* MkScopeCmd(Vars* vars, Ports* ports, Cmd* next);
static Cmd* MkJoinCmd(int count, Cmd* next);
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

// MkExprCmd --
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

static Cmd* MkExprCmd(const FblcExpr* expr, FblcValue** target, Cmd* next)
{
  assert(expr != NULL);
  assert(target != NULL);

  ExprCmd* cmd = GC_MALLOC(sizeof(ExprCmd));
  cmd->tag = CMD_EXPR;
  cmd->next = next;
  cmd->expr = expr;
  cmd->target = target;
  return (Cmd*)cmd;
}

// MkActnCmd --
//
//   Creates a command to evaluate the given action, storing the resulting
//   value at the given target location.
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

static Cmd* MkActnCmd(FblcActn* actn, FblcValue** target, Cmd* next)
{
  assert(actn != NULL);
  assert(target != NULL);

  ActnCmd* cmd = GC_MALLOC(sizeof(ActnCmd));
  cmd->tag = CMD_ACTN;
  cmd->next = next;
  cmd->actn = actn;
  cmd->target = target;
  return (Cmd*)cmd;
}

// MkAccessCmd --
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

static Cmd* MkAccessCmd(
    FblcValue* value, FblcName field, FblcValue** target, Cmd* next)
{
  AccessCmd* cmd = GC_MALLOC(sizeof(AccessCmd));
  cmd->tag = CMD_ACCESS;
  cmd->next = next;
  cmd->value = value;
  cmd->field = field;
  cmd->target = target;
  return (Cmd*)cmd;
}

// MkCondExprCmd --
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

static Cmd* MkCondExprCmd(
    FblcValue* value, FblcExpr* const* choices, FblcValue** target, Cmd* next)
{
  CondExprCmd* cmd = GC_MALLOC(sizeof(CondExprCmd));
  cmd->tag = CMD_COND_EXPR;
  cmd->next = next;
  cmd->value = value;
  cmd->choices = choices;
  cmd->target = target;
  return (Cmd*)cmd;
}

// MkCondActnCmd --
//   
//   Create a command to select and evaluate an action based on the tag of
//   the given value.
//
// Inputs:
//   value - The value to condition on.
//   choices - The list of actions to choose from based on the value.
//   target - The target destination for the final evaluated value.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkCondActnCmd(
    FblcValue* value, FblcActn** choices, FblcValue** target, Cmd* next)
{
  CondActnCmd* cmd = GC_MALLOC(sizeof(CondActnCmd));
  cmd->tag = CMD_COND_ACTN;
  cmd->next = next;
  cmd->value = value;
  cmd->choices = choices;
  cmd->target = target;
  return (Cmd*)cmd;
}

// MkScopeCmd --
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

static Cmd* MkScopeCmd(Vars* vars, Ports* ports, Cmd* next)
{
  ScopeCmd* cmd = GC_MALLOC(sizeof(ScopeCmd));
  cmd->tag = CMD_SCOPE;
  cmd->next = next;
  cmd->vars = vars;
  cmd->ports = ports;
  return (Cmd*)cmd;
}

// MkJoinCmd --
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

static Cmd* MkJoinCmd(int count, Cmd* next)
{
  JoinCmd* cmd = GC_MALLOC(sizeof(JoinCmd));
  cmd->tag = CMD_JOIN;
  cmd->next = next;
  cmd->count = count;
  return (Cmd*)cmd;
}

// MkPutCmd --
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

static Cmd* MkPutCmd(FblcValue** target, Link* link, Cmd* next)
{
  assert(target != NULL);
  PutCmd* cmd = GC_MALLOC(sizeof(PutCmd));
  cmd->tag = CMD_PUT;
  cmd->next = next;
  cmd->target = target;
  cmd->link = link;
  cmd->value = NULL;
  return (Cmd*)cmd;
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
//   cmds - The head of the command list for the thread.
//
// Result:
//   None.
//
// Side Effects:
//   Threads are added to the threads list based on the command executed. If
//   the command list is not executed to completion, a thread will be added to
//   the threads list representing the continuation of this thread.

static void Run(const FblcEnv* env, Threads* threads, Vars* vars, Ports* ports,
    Cmd* cmds)
{
  for (int i = 0; i < 1024 && cmds != NULL; i++) {
    switch (cmds->tag) {
      case CMD_EXPR: {
        ExprCmd* cmd = (ExprCmd*)cmds;
        const FblcExpr* expr = cmd->expr;
        FblcValue** target = cmd->target;
        cmds = cmds->next;
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
                *target = FblcNewValue(type);
                for (int i = 0; i < type->fieldc; i++) {
                  cmds = MkExprCmd(
                      expr->argv[i], &((*target)->fieldv[i]), cmds);
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
              // the vars for the scope command.

              // Don't put a scope change if we will immediately 
              // change it back to a different scope. This is important to
              // avoid memory leaks for tail calls.
              if (cmds != NULL && cmds->tag != CMD_SCOPE) {
                cmds = MkScopeCmd(vars, ports, cmds);
              }

              cmds = MkExprCmd(func->body, target, cmds);
              cmds = MkScopeCmd(NULL, ports, cmds);

              ScopeCmd* scmd = (ScopeCmd*)cmds;
              Vars* nvars = NULL;
              for (int i = 0; i < func->argc; i++) {
                FblcName var_name = func->argv[i].name.name;
                nvars = AddVar(nvars, var_name);
                cmds = MkExprCmd(
                    expr->argv[i], LookupRef(nvars, var_name), cmds);
              }
              scmd->vars = nvars;
              break;
            }
            assert(false && "No such struct type or function found");
          }

          case FBLC_ACCESS_EXPR: {
            // Add to the top of the command list:
            // object -> access -> ...
            AccessCmd* acmd = (AccessCmd*)MkAccessCmd(
                NULL, expr->ex.access.field.name, target, cmds);
            cmds = MkExprCmd(
                expr->ex.access.object, &(acmd->value), (Cmd*)acmd);
            break;
          }

          case FBLC_UNION_EXPR: {
            // Create the union value now, then add a command to evaluate the
            // argument of the union constructor and set the field of the
            // union value.
            FblcType* type = FblcLookupType(env, expr->ex.union_.type.name);
            assert(type != NULL);
            int tag = TagForField(type, expr->ex.union_.field.name);
            *target = FblcNewUnionValue(type, tag);
            cmds = MkExprCmd(
                expr->ex.union_.value, &((*target)->fieldv[0]), cmds);
            break;
          }

          case FBLC_LET_EXPR: {
            // Add to the top of the command list:
            // def -> var -> body -> (vars) -> ...

            // No need to reset the scope if we are going to switch to
            // different scope immediately after anyway.
            if (cmds != NULL && cmds->tag != CMD_SCOPE) {
              cmds = MkScopeCmd(vars, ports, cmds);
            }
            FblcName var_name = expr->ex.let.name.name;
            Vars* nvars = AddVar(vars, var_name);
            cmds = MkExprCmd(expr->ex.let.body, target, cmds);
            cmds = MkScopeCmd(nvars, ports, cmds);
            cmds = MkExprCmd(
                expr->ex.let.def, LookupRef(nvars, var_name), cmds);
            break;
          }

          case FBLC_COND_EXPR: {
            // Add to the top of the command list:
            // select -> econd -> ...
            CondExprCmd* ccmd = (CondExprCmd*)MkCondExprCmd(
                NULL, expr->argv, target, cmds);
            cmds = MkExprCmd(expr->ex.cond.select, &(ccmd->value), (Cmd*)ccmd);
            break;
          }
        }
        break;
      }

      case CMD_ACTN: {
        ActnCmd* cmd = (ActnCmd*)cmds;
        cmds = cmds->next;
        FblcActn* actn = cmd->actn;
        FblcValue** target = cmd->target;
        switch (actn->tag) {
          case FBLC_EVAL_ACTN: {
            cmds = MkExprCmd(actn->ac.eval.expr, target, cmds);
            break;
          }

          case FBLC_GET_ACTN: {
            Link* link = LookupPort(ports, actn->ac.get.port.name);
            assert(link != NULL && "Get port not in scope");

            if (link->head == NULL) {
              Thread* waiting = GC_MALLOC(sizeof(Thread));
              waiting->vars = vars;
              waiting->ports = ports;
              waiting->cmd = (Cmd*)cmd;
              waiting->next = link->waiting;
              link->waiting = waiting;
              cmds = NULL;
            } else {
              *target = link->head->value;
              link->head = link->head->next;
              if (link->head == NULL) {
                link->tail = NULL;
              }
            }
            break;
          }

          case FBLC_PUT_ACTN: {
            // expr -> put -> next
            Link* link = LookupPort(ports, actn->ac.put.port.name);
            assert(link != NULL && "Put port not in scope");
            PutCmd* pcmd = (PutCmd*)MkPutCmd(target, link, cmds);
            cmds = MkExprCmd(actn->ac.put.expr, &(pcmd->value), (Cmd*)pcmd);
            break;
          }

          case FBLC_CALL_ACTN: {
            FblcProc* proc = FblcLookupProc(env, actn->ac.call.proc.name);
            assert(proc != NULL && "No such proc found");
            assert(proc->portc == actn->ac.call.portc);
            assert(proc->argc == actn->ac.call.exprc);

            // Add to the top of the command list:
            // arg -> ... -> arg -> scope -> body -> (scope) -> ...
            // The results of the arg evaluations will be stored directly in
            // the vars for the scope command.

            // Don't put a scope change if we will immediately 
            // change it back to a different scope. This is important to
            // avoid memory leaks for tail calls.
            if (cmds != NULL && cmds->tag != CMD_SCOPE) {
              cmds = MkScopeCmd(vars, ports, cmds);
            }

            cmds = MkActnCmd(proc->body, target, cmds);

            Ports* nports = NULL;
            for (int i = 0; i < proc->portc; i++) {
              Link* link = LookupPort(ports, actn->ac.call.ports[i].name);
              assert(link != NULL && "port not found in scope");
              nports = AddPort(nports, proc->portv[i].name.name, link);
            }
            cmds = MkScopeCmd(NULL, nports, cmds);

            ScopeCmd* scmd = (ScopeCmd*)cmds;
            Vars* nvars = NULL;
            for (int i = 0; i < proc->argc; i++) {
              FblcName var_name = proc->argv[i].name.name;
              nvars = AddVar(nvars, var_name);
              cmds = MkExprCmd(actn->ac.call.exprs[i],
                  LookupRef(nvars, var_name), cmds);
            }
            scmd->vars = nvars;
            break;
          }

          case FBLC_LINK_ACTN: {
            Link* link = GC_MALLOC(sizeof(Link));
            link->head = NULL;
            link->tail = NULL;
            link->waiting = NULL;
            ports = AddPort(ports, actn->ac.link.getname.name, link);
            ports = AddPort(ports, actn->ac.link.putname.name, link);
            cmds = MkActnCmd(actn->ac.link.body, target, cmds);
            break;
          }

          case FBLC_EXEC_ACTN: {
            // Create new threads for each action to execute.
            // actn .>
            // actn ..> join -> scope -> actn -> scope -> next
            // actn .>
            cmds = MkScopeCmd(vars, ports, cmds);
            cmds = MkActnCmd(actn->ac.exec.body, target, cmds);
            ScopeCmd* scmd = (ScopeCmd*)MkScopeCmd(NULL, ports, cmds);
            Cmd* jcmd = MkJoinCmd(actn->ac.exec.execc, (Cmd*)scmd);

            Vars* nvars = vars;
            for (int i = 0; i < actn->ac.exec.execc; i++) {
              FblcExec* exec = &(actn->ac.exec.execv[i]);
              nvars = AddVar(nvars, exec->var.name.name);
              FblcValue** target = LookupRef(nvars, exec->var.name.name);
              AddThread(
                  threads, vars, ports, MkActnCmd(exec->actn, target, jcmd));
            }
            scmd->vars = nvars;
            cmds = NULL;
            break;
          }

          case FBLC_COND_ACTN: {
            // Add to the top of the command list:
            // select -> acond -> ...
            CondActnCmd* ccmd = (CondActnCmd*)MkCondActnCmd(
                NULL, actn->ac.cond.args, target, cmds);
            cmds = MkExprCmd(actn->ac.cond.select, &(ccmd->value), (Cmd*)ccmd);
            break;
          }
            break;
        }
        break;
      }               

      case CMD_ACCESS: {
        AccessCmd* cmd = (AccessCmd*)cmds;
        FblcType* type = cmd->value->type;
        int target_tag = TagForField(type, cmd->field);
        int actual_tag = cmd->value->tag;
        FblcValue** target = cmd->target;
        if (type->kind == FBLC_KIND_STRUCT) {
          *target = cmd->value->fieldv[target_tag];
        } else if (actual_tag == target_tag) {
          *target = cmd->value->fieldv[0];
        } else {
          fprintf(stderr, "MEMBER ACCESS UNDEFINED\n");
          abort();
        }
        cmds = cmds->next;
        break;
      }

      case CMD_COND_EXPR: {
        CondExprCmd* cmd = (CondExprCmd*)cmds;
        FblcValue* value = cmd->value;
        FblcValue** target = cmd->target;
        assert(value->type->kind == FBLC_KIND_UNION);
        cmds = MkExprCmd(cmd->choices[value->tag], target, cmd->next);
        break;
      }

      case CMD_COND_ACTN: {
        CondActnCmd* cmd = (CondActnCmd*)cmds;
        FblcValue* value = cmd->value;
        FblcValue** target = cmd->target;
        assert(value->type->kind == FBLC_KIND_UNION);
        cmds = MkActnCmd(cmd->choices[value->tag], target, cmd->next);
        break;
      }

      case CMD_SCOPE: {
        ScopeCmd* cmd = (ScopeCmd*)cmds;
        vars = cmd->vars;
        ports = cmd->ports;
        cmds = cmd->next;
        break;
      }

      case CMD_JOIN: {
        JoinCmd* cmd = (JoinCmd*)cmds;
        assert(cmd->count > 0);
        cmd->count--;
        cmds = cmd->count == 0 ? cmd->next : NULL;
        break;
      }

      case CMD_PUT: {
        PutCmd* cmd = (PutCmd*)cmds;
        Link* link = cmd->link;
        FblcValue** target = cmd->target;
        *target = cmd->value;
        assert(cmd->value != NULL);
        Values* ntail = GC_MALLOC(sizeof(Values));
        ntail->value = cmd->value;
        ntail->next = NULL;
        if (link->head == NULL) {
          assert(link->tail == NULL);
          link->head = ntail;
        } else {
          link->tail->next = ntail;
        }
        link->tail = ntail;
        cmds = cmd->next;

        if (link->waiting != NULL) {
          Thread* thread = link->waiting;
          AddThread(threads, thread->vars, thread->ports, thread->cmd);
          link->waiting = thread->next;
        }
        break;
      }
    }
  }

  if (cmds != NULL) {
    AddThread(threads, vars, ports, cmds);
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

  AddThread(&threads, NULL, NULL, MkActnCmd(actn, &result, NULL));

  Thread* thread = GetThread(&threads);
  while (thread != NULL) {
    Run(env, &threads, thread->vars, thread->ports, thread->cmd);
    thread = GetThread(&threads);
  }
  return result;
}
