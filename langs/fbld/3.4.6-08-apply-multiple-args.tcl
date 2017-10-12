set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      struct Pair(Unit a, Unit b);
      func foo(Unit a, Unit b ; Pair);
      func main( ; Pair);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();
      struct Pair(Unit a, Unit b);

      func foo(Unit a, Unit b ; Pair) {
        Pair(a, b);
      };

      func main( ; Pair) {
        # Test that we can call a function with multiple arguments.
        foo(Unit(), Unit());
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Pair@MainM(Unit@MainM(),Unit@MainM())
}
