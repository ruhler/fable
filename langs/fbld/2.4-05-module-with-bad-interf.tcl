set prg {
  interf I {
    # The type Blah is not defined.
    struct Bad(Blah x);
  };

  # The module has an interface with errors.
  module M(I) {
    priv struct Blah();

    struct Bad(Blah x);
  };
}

fbld-check-error $prg 4:16
