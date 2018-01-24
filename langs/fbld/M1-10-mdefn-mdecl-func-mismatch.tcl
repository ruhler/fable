set prg {
  interf MainI {
    struct Unit();
    func f(Unit x, Unit y; Unit);
  };

  module MainM(MainI) {
    struct Unit();

    # The declaration of 'f' here doesn't match the declaration in
    # Main.interf.
    func f(Unit x ; Unit) {
      Unit();
    };
  };
}

fbld-check-error $prg 12:10
