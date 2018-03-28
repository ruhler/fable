set prg {
  module A {
    struct A();
  };

  module B {
    import @ { M=A; };

    # There was a bug in the past where this would failed to report an error
    # message for the fact that 'A' is not in scope for the import.
    import A { A; };
  };
}

fbld-check-error $prg 11:12
