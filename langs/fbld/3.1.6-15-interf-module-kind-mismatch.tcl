set prg {
  interf I {
    struct Unit();
    func f(Unit x, Unit y; Unit);
  };

  module M(I) {
    struct Unit();

    # f should be a function according to the interface.
    struct f(Unit x, Unit y);
  };
}

fbld-check-error $prg 11:12
