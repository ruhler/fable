set prg {
  # Function declarations must have a semicolon separating inputs from outputs.
  struct Unit();

  func f(Unit x, Unit y Unit) {
    x;
  };
}
fblc-check-error $prg 5:25
