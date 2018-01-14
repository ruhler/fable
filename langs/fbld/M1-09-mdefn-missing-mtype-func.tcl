set prg {
  interf MainI {
    struct Unit();
    func f(Unit x ; Unit);
  };

  module MainM(MainI) {
    # The function f is not declared locally.
    struct Unit();
  };
}

fbld-check-error $prg 7:10
