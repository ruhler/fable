set prg {
  Main.mdecl {
    mdecl Main() {
      struct Unit();
      struct Pair(Unit a, Unit b);
      func foo(Unit a, Unit b ; Pair);
      func main( ; Pair);
    };
  }

  Main.mdefn {
    mdefn Main() {
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

fbld-test $prg main@Main {} {
  return Pair@Main(Unit@Main(),Unit@Main())
}
