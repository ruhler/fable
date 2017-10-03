set prg {
  # Function declarations must have a body.
  struct Unit();

  func f(Unit x, Unit y; Unit);
}
fblc-check-error $prg 5:31
