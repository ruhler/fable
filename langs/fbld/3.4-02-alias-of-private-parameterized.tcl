set prg {
  struct Unit();

  interf I<type T> {
    import @ { Unit; T; };
    struct Pair(T a, T b);
  };

  module M {
    import @ { Unit; I; };

    priv module MI<type R>(I<R>) {
      import @ { Unit; R; };
      struct Pair(R a, R b);
    };

    module UnitMI(I<Unit>) = MI<Unit>;
  };

  # Here the 'UnitMI' module is visible, but 'MI', which the alias refers
  # to, is not visible. This caused problems in the past because the compiler
  # failed to resolve the alias UnitMI -> MI<Unit> before looking up the type
  # parameter R.
  func main( ; Unit) {
    Pair@UnitMI@M(Unit(), Unit()).a;
  };
}

fbld-test $prg "main" {} {
  return Unit()
}
