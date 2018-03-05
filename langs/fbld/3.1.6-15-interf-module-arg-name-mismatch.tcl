set prg {
  interf I {
    struct Unit();
    func f(Unit x, Unit y; Unit);
  };

  module M(I) {
    struct Unit();

    # The argument name 'x' here doesn't match the declaration in interf I.
    func f(Unit z, Unit y; Unit) {
      Unit();
    };
  };
}

fbld-check-error $prg 11:17
