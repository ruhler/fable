The Fable Calvisus Test Suite
=============================

Each test case is described using a file. The file is a (possibly malformed)
Fblc program to test, with a main entry point called 'main' that takes no
input arguments.

The test file includes in comments a description of the test
being performed. It also includes a single line of the form:

-----
/// Expect: ...
-----

Where the '...' is replaced with the expected result of running the program.

There are two classes of tests, those with well formed Fblc programs and those
with malformed Fblc programs. The malformed Fblc programs will have ERROR as
there expected output:

-----
/// Expect: ERROR
-----

The tests for well formed and malformed Fblc programs can also be
distinguished by the name of the test file. Tests for well formed programs
have file names starting with "####v-", while tests for malformed programs have
file names starting with "####e-".

THe tests are organized into the following ranges. Each test has a unique
number.

Test              0000-0099

Struct Decl       1000-1099
Union Decl        1100-1199
Func Decl         1200-1299
Proc Decl         1300-1399

Var Expr          2000-2099
Struct Expr       2100-2199
Union Expr        2200-2299
Access Expr       2300-2399
Cond Expr         2400-2499
App Expr          2500-2599
Let Expr          2600-2699

Get Proc          3000-3099
Put Proc          3100-3199
Call Proc         3200-3299
Cond Proc         3300-3399
Link Proc         3400-3499
Let Proc          3500-3599

Prog              4000-4099


TODO
~~~~

Here are some more tests to write or port to the new system:

duplicate-struct-field.fblc   // STRUCT DECL
no-such-type.fblc             // FUNC DECL

no-such-var.fblc              // VAR
no-such-tag.fblc                  // UNION
struct-type-in-union-literal.fblc // UNION
no-such-func.fblc             // APP
no-such-field.fblc            // ACCESS
inconsistent-cond-type.fblc   // COND
too-few-cond-args.fblc        // COND
bad-assign-type.fblc          // LET

too-few-func-args.fblc
too-few-struct-args.fblc
too-few-union-args.fblc
too-many-cond-args.fblc
too-many-func-args.fblc
too-many-struct-args.fblc
too-many-union-args.fblc
undefined-type-in-union-literal.fblc
union-type-as-function.fblc
var-shadow-arg.fblc
var-shadow-var.fblc
wrong-type-cond-select.fblc
wrong-type-func-args.fblc
wrong-type-func-return.fblc
wrong-type-struct-args.fblc
wrong-type-union-args.fblc
  - have an error in the object expression for an access expression.
  - have an error in the value expression for a union literal.
  - have a struct field refer to a non-existant type.
  - have a function argument refer to a non-existant type.
  - have a function result refer to a non-existant type.
  - define a union type with no fields.
  - syntax errors:
    +  missing field name in struct declaration or function call.
    +  Missing '(' after the '?' in a conditional
    +  Missing ')' after an argument list.
    +  Parse error in conditional args list.
    +  Missing field in access expression.
