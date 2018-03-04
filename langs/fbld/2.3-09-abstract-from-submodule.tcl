set prg {
  struct Unit();

  module A {
    import @ { Unit; };

    abst struct Pair(Unit first, Unit second);

    module B {
      import @ { Unit; Pair; };

      func foo(Pair x; Unit) {
        # The 'first' field of Pair can be accessed, because Pair is defined in
        # the parent module.
        x.first;
      };

      func main( ; Unit) {
        foo(Pair(Unit(), Unit()));
      };
    };
  };
}

fbld-test $prg "main@B@A" {} {
  return Unit()
}
