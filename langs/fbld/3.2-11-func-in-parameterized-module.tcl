set prg {
  # Verify we can correctly compile functions defined in parameterized
  # modules. This is a regression test for a bug we had in the past.
  module M<type T> {
    import @ { T; };

    struct Pair(T a, T b);

    func MkPair(T a, T b; Pair) {
      Pair(a, b);
    };
  };

  struct Unit();

  func main( ; Unit) {
    MkPair@M<Unit>(Unit(), Unit()).a;
  };
}

fblc-test $prg main {} "return Unit()"
