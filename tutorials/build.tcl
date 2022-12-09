namespace eval "tutorials" {
  # FirstProgram tests
  test $::b/tutorials/FirstProgram.tr \
    "$::b/test/fble-test $::s/tutorials/FirstProgram/Hello.fble" \
    "$::b/test/fble-test -I $::s/tutorials/FirstProgram -m /Hello%"


  # MainDriver-1 tests
  build $::b/tutorials/MainDriver-1/hello \
    "$::s/tutorials/MainDriver-1/hello.c $::b/lib/libfble.a" \
    "gcc -o $::b/tutorials/MainDriver-1/hello $::s/tutorials/MainDriver-1/hello.c -I $::s/include -L $::b/lib -lfble"
  set hello [file normalize $::b/tutorials/MainDriver-1/hello]
  set hello_out [file normalize $::b/tutorials/MainDriver-1/hello.out]
  build $::b/tutorials/MainDriver-1/hello.out \
    "$::b/tutorials/MainDriver-1/hello $::s/tutorials/MainDriver-1/Hello.fble" \
    "cd $::s/tutorials/MainDriver-1 ; $hello > $hello_out"
  build $::b/tutorials/MainDriver-1/hello.wnt "" \
    "echo Result: 0010 > $::b/tutorials/MainDriver-1/hello.wnt"
  test $::b/tutorials/MainDriver-1.tr \
    "$::b/tutorials/MainDriver-1/hello.wnt $::b/tutorials/MainDriver-1/hello.out" \
    "cmp $::b/tutorials/MainDriver-1/hello.wnt $::b/tutorials/MainDriver-1/hello.out"

  # MainDriver-2 tests
  build $::b/tutorials/MainDriver-2/hello \
    "$::s/tutorials/MainDriver-2/hello.c $::b/lib/libfble.a" \
    "gcc -o $::b/tutorials/MainDriver-2/hello $::s/tutorials/MainDriver-2/hello.c -I $::s/include -L $::b/lib -lfble"
  set hello [file normalize $::b/tutorials/MainDriver-2/hello]
  set hello_out [file normalize $::b/tutorials/MainDriver-2/hello.out]
  build $::b/tutorials/MainDriver-2/hello.out \
    "$::b/tutorials/MainDriver-2/hello $::s/tutorials/MainDriver-2/Hello.fble" \
    "cd $::s/tutorials/MainDriver-2 ; $hello 0011 1010 > $hello_out"
  build $::b/tutorials/MainDriver-2/hello.wnt "" \
    "echo Result: 0010 > $::b/tutorials/MainDriver-2/hello.wnt"
  test $::b/tutorials/MainDriver-2.tr \
    "$::b/tutorials/MainDriver-2/hello.wnt $::b/tutorials/MainDriver-2/hello.out" \
    "cmp $::b/tutorials/MainDriver-2/hello.wnt $::b/tutorials/MainDriver-2/hello.out"

  # CompiledCode tests
  build $::b/tutorials/CompiledCode/Hello.fble.c \
    "$::b/bin/fble-compile $::s/tutorials/CompiledCode/Hello.fble"\
    "$::b/bin/fble-compile -t c -c -e HelloModule -I $::s/tutorials/CompiledCode -m /Hello% > $::b/tutorials/CompiledCode/Hello.fble.c"
  build $::b/tutorials/CompiledCode/hello \
    "$::s/tutorials/CompiledCode/hello.c $::b/tutorials/CompiledCode/Hello.fble.c $::b/lib/libfble.a" \
    "gcc -o $::b/tutorials/CompiledCode/hello $::s/tutorials/CompiledCode/hello.c $::b/tutorials/CompiledCode/Hello.fble.c -I $::s/include -L $::b/lib -lfble"
  set hello [file normalize $::b/tutorials/CompiledCode/hello]
  set hello_out [file normalize $::b/tutorials/CompiledCode/hello.out]
  build $::b/tutorials/CompiledCode/hello.out \
    "$::b/tutorials/CompiledCode/hello $::s/tutorials/CompiledCode/Hello.fble" \
    "cd $::s/tutorials/CompiledCode ; $hello 0011 1010 > $hello_out"
  build $::b/tutorials/CompiledCode/hello.wnt "" \
    "echo Result: 0010 > $::b/tutorials/CompiledCode/hello.wnt"
  test $::b/tutorials/CompiledCode.tr \
    "$::b/tutorials/CompiledCode/hello.wnt $::b/tutorials/CompiledCode/hello.out" \
    "cmp $::b/tutorials/CompiledCode/hello.wnt $::b/tutorials/CompiledCode/hello.out"
}
