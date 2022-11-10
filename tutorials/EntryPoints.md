# Entry Points

This tutorial walks you through how to write a main function for the
fble program from FirstProgram.md. By the end of the tutorial you'll be able
to pass bit vectors on the command line and see the results of bitwise
operations printed out.

## No Main Function

Unlike many other languages, the fble language doesn't have a concept of a
`main` function. There are no built-in integer, string, or array types, and
there is no access to operating system calls from the language for reading and
writing files. Instead, you can write C code to define your own main
functions.

An fble program describes how to evaluate an fble value. The value could be a
struct value, a union value, or a function value, for example. What to do with
the resulting value is up to driver code written in C. We've already seen one
example of fble driver code: the `fble-test` program, which evaluates the
program and discards the result.

In FirstProgram.md we wrote a program that computes the bitwise `AND` of two 4-bit
bit vectors. Let's write some driver code that runs the program and prints out
whatever `Bit4@` value is returned by the program.

We'll use the same fble program from before, but instead of returning `Unit`,
return the value `z` directly:

    @ Unit@ = *();
    @ Bit@ = +(Unit@ 0, Unit@ 1);
    @ Bit4@ = *(Bit@ 3, Bit@ 2, Bit@ 1, Bit@ 0);

    Unit@ Unit = Unit@();

    Bit@ 0 = Bit@(0: Unit);
    Bit@ 1 = Bit@(1: Unit);

    Bit4@ x = Bit4@(0, 0, 1, 1);
    Bit4@ y = Bit4@(1, 0, 1, 0);

    (Bit@, Bit@) { Bit@; } And = (Bit@ a, Bit@ b) {
      a.?(0: 0, 1: b);
    };

    (Bit4@, Bit4@) { Bit4@; } And4 = (Bit4@ a, Bit4@ b) {
      Bit4@(And(a.3, b.3), And(a.2, b.2), And(a.1, b.1), And(a.0, b.0));
    };

    Bit4@ z = And4(x, y);

    z;

## Main Driver Code

TODO

   gcc -o tutorial2a Tutorial2a.c -I ../install/include -L ../install/lib -lfble 
