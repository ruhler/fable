# Tutorial 2: First Program Fundamentals

In this tutorial we revisit the program from tutorial 1 in depth, for those
interested in learning more about the fundamentals of the fble language. Feel
free to skip this tutorial the first go around, because it covers advanced
topics that may be easier to grasp after having more practice writing basic
fble programs.

This tutorial assumes you have already gone through tutorial 1.

## Types are Values

In tutorial 1 we defined three types, `Unit@`, `Bit@`, and `Bit4`:

    @ Unit@ = *();
    @ Bit@ = +(Unit@ 0, Unit@ 1);
    @ Bit4@ = *(Bit@ 3, Bit@ 2, Bit@ 1, Bit@ 0);

In reality, we aren't defining types here, we are defining variables whose
values are types. `Unit@` is a variable whose value is the type `*()`. `Bit@`
is a variable whose value is the type `+(Unit@ 0, Unit@ 1)`. We give names to
the types because it is convenient, not because it is necessary.

Anywhere we use the variable `Unit@`, we could have instead written
`*()`. For example, we could have defined `Bit@` using:

    @ Bit@ = +(*() 0, *() 1);

We could, if we wanted, have defined `Bit4@` using:

    @ Bit4@ = *(
        +(*() 0, *() 1) 3,
        +(*() 0, *() 1) 2,
        +(*() 0, *() 1) 1,
        +(*() 0, *() 1) 0);

It's still the same type, regardless of which way we define it.

As another example, we could define two different variables with the same type
value:

    @ BitA@ = +(Unit@ 0, Unit@ 1);
    @ BitB@ = +(Unit@ 0, Unit@ 1);

Both `BitA@` and `BitB@` refer to the same type. You can use them
interchangeably. For example, the following is perfectly valid:

    BitA@ 0 = BitB@(0: Unit);
    BitB@ 1 = BitA@(1: Unit);

This is different from languages like C. In C, the following definitions would
produce two different types:

    struct Bit4A { Bit bit3, Bit bit2, Bit bit1, Bit bit0 };
    struct Bit4B { Bit bit3, Bit bit2, Bit bit1, Bit bit0 };

You could not use Bit4A and Bit4B interchangably in C.

### The Type of a Type

We've said `Bit@` is a variable whose value is a type. Then what is the type
of the variable `Bit@`? The type of a type is a type uniquely determined by
the value of the type. There's much more to say than that. You can refer to
the type of a variable using the `@<>` operator in fble, so we could say the
type of `Bit@` is exactly `@<Bit@>`.

## Variable Declarations

We've seen two different ways to declare variables so far. For type variables,
we have something like:

    @ Unit@ = *();

For normal variables, we have something like:

    Unit@ Unit = Unit@();

Each of these two forms of variable declarations can be used both for type
variables and normal variables. The real difference between these two forms of
variable declaration is whether the type of the variable is specified
explicitly or not.

In the normal variable declaration above, the type of the variable is given
explicitly as `Unit@`. We could use that form for declaring a type variable
too if we wanted, knowing that `@<...>` can be used to get the type of a type.
For example, we could define `Unit@` using:

    @<*()> Unit@ = *();

We don't tend to do this in practice because its redundant and syntactically
awkward.

In the type variable declaration, the type of the variable is implicit.
Instead of writing down a type, we write down the kind of the variable. The
kind of a type is `@`, and the kind of a value is `%`. We could define the
`Unit` variable this way:

    % Unit = Unit@();

The type of `Unit` will automatically be inferred from its value `Unit@()`. In
theory you could always use implicit type variable declarations if you wanted,
but I don't recommend it. Be kind to the readers of your program and put
explicit types so they don't have to figure out the types themselves from the
code.

## Lexical Scoping

In tutorial 1 we defined an `And` function:

    (Bit@, Bit@) { Bit@; } And = (Bit@ a, Bit@ b) {
      a.?(0: 0, 1: b);
    };

Notice the body of the function references arguments `a` and `b` as well as
the globally defined variable `0`. The function is lexically scoped. The value
of `0` when you execute the function is the value of `0` when the function is
defined.

Consider the following program assuming some type `Int@`, values `4` and
`5`, and an `Add` function:

    Int@ x = 4;
    (Int@) { Int@; } Add4 = (Int@ a) {
      Add(a, x);
    };

    Int@ x = 5;
    (Int@) { Int@; } Add5 = (Int@ a) {
      Add(a, x);
    };

    Int@ 8 = Add4(4);
    Int@ 9 = Add5(4);
    
The function `Add4` will add 4 to its argument, because the value of `x` is
`4` when the function is defined. Regardless of when the function is called.

An implication of this is that functions capture free variables from their
environment. If instead of `Int@`, `x` was some large structure, that
structure would stay in memory as long as the function `Add4` was still in
memory.
