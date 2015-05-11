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

The tests are organized into the following ranges. Each test has a unique
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
Write more tests.
