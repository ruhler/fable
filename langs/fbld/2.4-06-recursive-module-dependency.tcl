set prg {
  struct Unit();

  module A {
    import @ { Unit; S@B; };

    # Type is recursively defined against module B.
    struct P(Unit head, S tail);
  };

  module B {
    import @ { Unit; P@A; };

    # Type is recursively defined against module A.
    union S(P cons, Unit nil);
  };
}

# TODO: Where should the error be reported?
fbld-check-error $prg 12:24
