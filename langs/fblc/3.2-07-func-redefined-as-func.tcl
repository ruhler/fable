set prg {
  # A function must not be declared multiple times.
  struct Unit();

  func foo(Unit x ; Unit) {
    x;
  };

  func foo(Unit x, Unit y; Unit) {
    y;
  };
}
fblc-check-error $prg 9:8
