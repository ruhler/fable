set prg {
  interf StdLibI {
    struct Unit();
  };

  module M {
    import @ { StdLibI; };

    priv module StdLib(StdLibI) {
      struct Unit();
    };

    module Aliased(StdLibI) = StdLib;
  };

  # Here the 'Aliased' module is visible, but 'StdLib', which the alias refers
  # to, is not visible.
  func main( ; Unit@Aliased@M) {
    Unit@Aliased@M();
  };
}

fbld-test $prg "main" {} {
  return Unit@Aliased@M()
}
