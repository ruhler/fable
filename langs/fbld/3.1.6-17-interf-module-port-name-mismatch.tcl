set prg {
  interf I {
    struct Unit();
    proc p(Unit+ out; ; Unit);
  };

  module M(I) {
    struct Unit();

    # The name of the port should be 'out', not 'x'.
    proc p(Unit+ x; ; Unit) {
      $(Unit());
    };
  };
}

fbld-check-error $prg 11:18
