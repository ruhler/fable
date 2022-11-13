# Entry Points, Part I

This tutorial walks you through how to write a main function for the
fble program from FirstProgram.md.

This is part 1 of a two part tutorial. The first part shows how to print the
result of the bitwise computation to the screen in a user friendly way. The
second part shows how to pass bit vectors on the command line to use in the
bitwise operation.

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

In your favorite text editor, start a new file called `hello.c`.

### Includes

We'll start with some boiler plate and includes for the fble library:

    #include <stdio.h>
    #include <fble/fble.h>

    int main(int argc, const char* argv[])
    {

The header file `fble/fble.h` defines all the public C API functions for fble.
We include `stdio.h` for printf, and start a standard C main function
definition.

### Search Path

As we saw in FirstProgram.md, one of the inputs when running an fble program
is the paths to look for fble code. For the sake of this tutorial we will hard
code the search path with the equivalent of a `-I .` option:

    FbleSearchPath search_path;
    FbleVectorInit(search_path);
    FbleVectorAppend(search_path, ".");

`FbleSearchPath` is a type representing an vector of strings, where we are
using fble's own vector facilities. After initializing the vector, we append
the single element `"."`.

FbleVectorInit allocates memory for the vector under the covers; we'll need
to call `FbleVectorFree(search_path)` when we are done with the search path to
free up that memory.

### Module Path

For this tutorial, we'll hard code the module path to `/Hello%`:

    FbleModulePath* module_path = FbleParseModulePath("/Hello%");
    if (module_path == NULL) {
      fprintf(stderr, "Failed to parse module path.\n");
      return 1;
    }

The error case will not be triggered in this case, because "/Hello%" is valid
syntax for a module path. `FbleParseModulePath` allocates memory for
`module_path`; we'll need to call `FbleFreeModulePath` when we are done with
the allocated module path.

### Loading the Program

The next step is to load the fble program from source code:

    FbleValueHeap* heap = FbleNewValueHeap();
    FbleValue* linked = FbleLinkFromSource(heap, search_path, module_path, NULL);
    FbleFreeModulePath(module_path);
    FbleVectorFree(search_path);

We start by calling `FbleNewValueHeap` to allocate a new garbage collected
heap of fble values. The heap keeps track of allocate values and automatically
cleans up any that are no longer referenced.

Next we load the fble code from source using `FbleLinkFromSource`, passing in
the heap we just allocated, the search path, and the module path. The fourth
argument to `FbleLinkFromSource` is used for profiling, which we will not
discuss in this tutorial, so we'll leave it as `NULL`.

`FbleLinkFromSource` search for the `.fble` file corresponding to the module
path, in this case `./Hello.fble` for search path `.` and module path
`/Hello%`. It loads the program, performs type checking, compiles to an
internal bytecode format, and returns an `FbleValue*` representing the loaded
module. `FbleValue*` is used for all kinds of fble values, including struct
values, union values, and function values. In this case, `linked` is a special
zero-argument fble function value that can be evaluated using `FbleEval`.

Once we have loaded the program, we no longer need to hold on to `search_path`
or `module_path`, so we free resources associated with those.

### Checking for Errors

If you have a syntax error or a type error in your program,
`FbleLinkFromSource` will return `NULL`. It's important that we check for
this, clean up, and return in this case:

    if (linked == NULL) {
      FbleFreeValueHeap(heap);
      return 1;
    }

There's no need to print an error message, because `FbleLinkFromSource` will
have printed an error message to `stderr` in case of an error.

### Evaluating the Program

We use `FbleEval` to evaluate the program. This runs the code for the program
and returns the result:

    FbleValue* result = FbleEval(heap, linked, NULL);
    FbleReleaseValue(heap, linked);

    if (result == NULL) {
      FbleFreeValueHeap(heap);
      return 1;
    }

As before, we pass `NULL` for the profiling argument. We call
`FbleReleaseValue` on `linked` after evaluating the program to tell the
garbage collector it no longer needs to hold on to the `linked` value. And
finally we check if the result is `NULL`, which would indicate a runtime error
occurred.

### Printing the Result

The resulting value should have type `Bit4@`. We can iterate over each field
of that structure and print whether the bits are `0` or `1` using the
following code:

    printf("Result: ");
    for (size_t i = 0; i < 4; ++i) {
      FbleValue* bit = FbleStructValueAccess(result, i);
      printf("%c", FbleUnionValueTag(bit) == 0 ? '0' : '1');
    }
    printf("\n");

The `FbleStructValueAccess` function accesses a field of a struct by
position. The `FbleUnionValueTag` function returns the tag of a union value.

### Cleaning up

When we are done, we need to release the `result` value, free the `heap`, and
return from our main function:

    FbleReleaseValue(heap, result);
    FbleFreeValueHeap(heap);
    return 0;
  }

And that's it! You have finished writing driver code to run your fble program
with the interpreter and print out the resulting `Bit4@` value.

## Running the Code

To try running the code, we need to compile `hello.c` into a binary, and then
invoke that binary:

   $ gcc -o hello hello.c -lfble 
   $ ./hello

Whether this works depends on where the include files and library files for
fble are installed. If your include files or library files are installed at in
a non-standard path, you may need to use the `-I` and `-L` options to gcc. For
example, if you have built fble but not yet installed it, the include files
will be in the source directory under include/ and the library will be in your
build directory under lib/. Assuming $SOURCE points to your source directory
and $BUILD points to your build directory, you would want:

   $ gcc -o hello hello.c -I $SOURCE/include -L $BUILD/lib -lfble 

Because we hard coded `.` as the search path, you'll also need to run your
`hello` binary from the same directory where `Hello.fble` lives.

If all goes well, you should see:

    $ ./hello
    $ Result: 0010

## Exercises

1. Change the values of `x` and `y` in `Hello.fble` and rerun the `hello`
   program. Does the output match what you expect?
2. Change your `hello` program to take the search path and module path as
   command line arguments. Try running `hello` from a different directory and
   using a different name for the `Hello.fble` file.
3. Define a `Bit8@` type as a structure with two `Bit4@` fields. Update
   `Hello.fble` to do bitwise `And8` on `Bit8@` values and update `hello.c` to
   print the resulting 8-bit value as output.

## Next Steps

Head over to EntryPoints-2.md to learn how to pass `x` and `y` on the command
line when running the `hello` program.
