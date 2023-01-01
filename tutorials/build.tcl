namespace eval "tutorials" {
  foreach {x} [glob $::s/tutorials/*.fbld] {
    set base [file rootname [file tail $x]]
    test $::b/tutorials/$base.fbld.tr \
      "$::s/tutorials/$base.fbld $::s/fbld/fbld.tcl $::s/fbld/core.tcl $::s/fbld/frontends/tutorial.tcl" \
      "tclsh8.6 $::s/fbld/frontends/tutorial.tcl $::s/tutorials/$base.fbld"
  }

  # HelloWorld tests
  test $::b/tutorials/HelloWorld.tr \
    "$::b/pkgs/core/fble-stdio $::s/tutorials/HelloWorld/HelloWorld.fble" \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -I $::s/tutorials/HelloWorld -m /HelloWorld%"

  # Basics tests
  test $::b/tutorials/Basics.tr \
    "$::b/pkgs/core/fble-stdio $::s/tutorials/Basics/Basics.fble" \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -I $::s/tutorials/Basics -m /Basics%"

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

  # Modules tests
  build $::b/tutorials/Modules/hello \
    "$::s/tutorials/Modules/hello.c $::b/lib/libfble.a" \
    "gcc -o $::b/tutorials/Modules/hello $::s/tutorials/Modules/hello.c -I $::s/include -L $::b/lib -lfble"
  set hello [file normalize $::b/tutorials/Modules/hello]
  set hello_out [file normalize $::b/tutorials/Modules/hello.out]
  build $::b/tutorials/Modules/hello.out [list \
    $::b/tutorials/Modules/hello \
    $::s/tutorials/Modules/Unit.fble \
    $::s/tutorials/Modules/Bits/Bit.fble \
    $::s/tutorials/Modules/Bits/Bit4.fble \
    $::s/tutorials/Modules/Hello.fble] \
    "cd $::s/tutorials/Modules ; $hello > $hello_out"
  build $::b/tutorials/Modules/hello.wnt "" \
    "echo Result: 0010 > $::b/tutorials/Modules/hello.wnt"
  test $::b/tutorials/Modules.tr \
    "$::b/tutorials/Modules/hello.wnt $::b/tutorials/Modules/hello.out" \
    "cmp $::b/tutorials/Modules/hello.wnt $::b/tutorials/Modules/hello.out"
}
