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
}
