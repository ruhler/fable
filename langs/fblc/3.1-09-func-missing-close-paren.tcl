set prg {
  # Function declarations must have a close paren.
  struct Unit();

  func f(Unit x, Unit y; Unit {
    x;
  };
}
fblc-check-error $prg 5:31
