set prg {
  interf I {
    struct Unit();
    proc p(Unit+ out; ; Unit);
  };

  module M(I) {
    struct Unit();

    # The proc p should have one port, not two.
    proc p(Unit+ out, Unit- in; ; Unit) {
      $(Unit());
    };
  };
}

fbld-check-error $prg 11:10
