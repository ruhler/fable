# First Program

This tutorial walks you through writing and testing your first fble program.
You'll implement a bitwise `AND` operation on a 4 bit bit-vector. To
better focus on the basics, the program is written from scratch, without
making use of any existing libraries.

In your favorite text editor, start a new file called `Hello.fble`. The
`.fble` extension is used for fble programs, and required for the fble
interpreter and compiler to be able to locate your code.

## Types

We'll start by defining types for a single bit and a 4 bit bit-vector:

    @ Unit@ = *();
    @ Bit@ = +(Unit@ 0, Unit@ 1);
    @ Bit4@ = *(Bit@ 3, Bit@ 2, Bit@ 1, Bit@ 0);

Actually, this code defines three types, named `Unit@`, `Bit@`, and `Bit4@`.
The language requires type names to end with the `@` character, to distinguish
them from normal variable names.

The `@` at the beginning of each line says we are defining names of types.
The syntax `*(...)` is used for struct types and `+(...)` for union types. 

The `Unit@` type defined here is a struct type without any fields. We call it
`Unit@` because there's only one possible value for the type. It is a building
block for the `Bit@` type. `Unit@` is equivalent to the `()` type in Haskell,
or the type of a zero-element tuple `()` in Python.

The `Bit@` type is defined as a union of two `Unit@` types, labeled `0` and
`1`. There are two possible values for the `Bit@` type, a `Unit@` value tagged
with `0` and a `Unit@` value tagged with `1`. Union values implicitly keep track
of their tag, unlike in C. You can ask a union value what its tag is and
access the value associated with the tag. More on that later. There is nothing
special about the names of the fields `0` and `1` that we have chosen here. We
could just as well have named them `zero` and `one`. The fble language has no
notion of built in numbers, which means you are free to use numbers as
identifiers for variable names or field names.

`Bit4@` is a struct type with four fields, named `3`, `2`, `1`, and `0`.
Each field has type `Bit@`. This represents a bit vector of 4 bits. We'll call
bit 3 the most significant bit and put it on the left as is conventional, but
it doesn't really matter for the bitwise operations defined in this tutorial.

## Values

Now that we have the `Bit@` and `Bit4@` types defined, we can declare some
variables of those types:

    Unit@ Unit = Unit@();

    Bit@ 0 = Bit@(0: Unit);
    Bit@ 1 = Bit@(1: Unit);

    Bit4@ x = Bit4@(0, 0, 1, 1);
    Bit4@ y = Bit4@(1, 0, 1, 0);

This code declares 5 variables, named `Unit`, `0`, `1`, `x`, and `y`. The
variable declarations start with the type of the variable, followed by the
name of the variable, `=`, and the value of the variable.

The syntax to create a struct value is the struct type followed by the list of
values for each field of the struct, in order, in parenthesis. The syntax to
create a union value is the union type followed by the tag, a colon, and the
value to use for that tagged field all in parenthesis.

Union and struct values are immutable, lightweight, and passed by value. A
decent implementation of fble will implement a value of `Bit4@` like `x` or
`y` easily in a single machine word.

## Functions

We'll define two functions in our first program: A function `And` that
computes the `AND` operation on individual bits, and a function `And4` to
compute bitwise `AND` on our 4 bit bit-vectors:

    (Bit@, Bit@) { Bit@; } And = (Bit@ a, Bit@ b) {
      a.?(0: 0, 1: b);
    };

    (Bit4@, Bit4@) { Bit4@; } And4 = (Bit4@ a, Bit4@ b) {
      Bit4@(And(a.3, b.3), And(a.2, b.2), And(a.1, b.1), And(a.0, b.0));
    };

The functions `And` and `And4` are variables just like `Unit`, `0`, `1`, `x`,
and `y`. To declare these variables, we start with the type, followed by the
name, `=`, and the their value. In this case we have function types and
function values instead of struct or union types and struct or union values.

The syntax for a function type is the list of argument types in parenthesis,
followed by the return type of the function in braces. The type for the `And`
function is `(Bit@, Bit@) { Bit@; }`, which means it is a function that takes
two arguments of type `Bit@` and returns a `Bit@` as a result.

The syntax for a function value is a list of named arguments in parenthesis
followed by the body of the function. The implementation of the `And` function
uses names `a` and `b` for the arguments. The body of the function consists of
a single expression, which will be evaluated to compute the result of the
function.

In the case of the `And` function, the result is computed using the
conditional expression `a.?(0: 0, 1: b)`. Conditional expressions are similar
to the conditional operator, if statements, or case statements in C and other
languages. The syntax for a conditional expression is the union value to
switch on, followed by `.?`, followed by an expression to evaluate for each
possible tag of the union value. At runtime, the tag associated with the union
value is used to select the expression to return. In this case, if `a` is
tagged with `0`, the result `0` will be returned, and if `a` is tagged with
`1`, the result `b` will be returned. That defines an `AND` operation.

The `And4` function is similarly defined. It returns a `Bit4@` value whose
fields are the result of applying the `And` function to the corresponding
fields of the arguments `a` and `b`. For example, `And(a.3, b.3)` calls the
`And` function with bit 3 of `a` and bit 3 of `b`, and uses the result for bit
3 of the result of the `And4` function.

## Testing the Result

Now that we've defined our bitwise `And4` operation, let's test the result of
running it on our previously defined `Bit4@` values `x` and `y`:

    Bit4@ z = And4(x, y);

    Unit@ _ = z.3.0;
    Unit@ _ = z.2.0;
    Unit@ _ = z.1.1;
    Unit@ _ = z.0.0;

    Unit;

We define the variable `z` as the bitwise `AND` of `x` and `y`. Because `x` is
`0011` and `y` is `1010`, we expect `z` to be `0010`.

We aren't using any standard library code in this tutorial, so we don't have
an easy way to print out the result of `z`. We'll show how to do that in
future tutorials. For now, we use a bit of a hack to test what the value of
`z` is.

You saw earlier that you can use notation such as `a.3` to access a field of a
struct value. You can use the same notation to access a field of a union
value. When accessing the field of a union, you can only access the field
corresponding to the union value's current tag, otherwise you'll get a runtime
error. For example, consider the union value `0`. This is a value `Unit`
tagged with `0`. The expression `0.0`, then, will give you back the value
`Unit`. The expression `0.1` will result in a runtime error, because `0` is
tagged with `0`, not `1`.

To test the value of `z`, we'll use union field access to access the expected
field from each bit of `z`. If the fields have the values we expect, there
will be no runtime error when running this program. Otherwise there will be a
runtime error.

The syntax `Unit@ _ = ...` defines a variable named `_` whose result we will
ignore. We use `_` for unused values by convention. The fble interpreter and
compiler will give a warning for unused variables if they don't start with an
underscore. It's okay to define multiple variables with the same name in fble.
In this case subsequent definitions of the variable will shadow previous
definitions of the value.

We end the program with `Unit;`. An fble program describes a value. In this
case, we only care that the program runs without getting a runtime error, so
it doesn't matter what value we return from the program.

## Running the program.

We've finished writing our first fble program! The next step is to try running
it. Our fble implementation comes with an executable called `fble-test` that
can be used to run fble programs. `fble-test` runs an fble program and
discards its result. It will report any syntax, compiler, or runtime errors
encountered.

Try running your program using the following:

    fble-test -I . -m /Hello%

**Note:** If you have built fble yourself and haven't yet installed
`fble-test`, it should be available at the path `fble/test/fble-test` in your
build directory.

The `-I .` option says to look in the current directory  for your
`Hello.fble`. You could change this to `-I tutorials/FirstProgram`, for
example, to try running the `tutorials/FirstProgram/Hello.fble` program
included alongside this tutorial instead.

The `-m /Hello%` option says which fble module to run. We haven't
introduced modules in this tutorial, but briefly, `Hello.fble` defines a
module referenced by the module path `/Hello%` from the include directory
specified by the `-I` option discussed above. The `fble-test` program will
convert the module path `/Hello%` to the file name `Hello.fble`
and search for that file to run in the given include path.

If all goes well, nothing should be printed out. That means there weren't any
errors. To force an error, change one of the `Unit@ _ = ...` lines. For
example, change `Unit@ _ = z.3.0;` to `Unit@ _ = z.3.1;`. Rerun the command
above, and you should get an error message:

> Hello.fble:29:15: error: union field access undefined: wrong tag

This error message means you tried to access a union field that was different
from the tag associated with the union value. In this case, the union value
was tagged with `0`, and you tried to access its field `0`.

## Exercises

1. Try implementing bitwise `NOT`, `OR` and `XOR` functions.
2. Try defining a `Bit8@` type and implementing a bitwise `And8` operation for
   it. You could reuse `Bit4@` for the definition of `Bit8@` or just use
   `Bit@` like how we defined `Bit4@`.
3. Try implementing a 4-bit `Add` function that can add two `Bit4@`
   interpreted as twos-complement integers.

## Next Steps

Head over to MainDriver-1.md to learn how to output the bit vector value
computed by your program.

