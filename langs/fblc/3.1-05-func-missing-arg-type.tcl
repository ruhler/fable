set prg {
  # Function declarations must have types for each argument.
  struct Unit();

  func f(x, Unit y; Unit) {
    x;
  };
}
fblc-check-error $prg 5:11
