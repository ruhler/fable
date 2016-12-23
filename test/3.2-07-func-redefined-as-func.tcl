

# A function must not be declared multiple times.
set prg {
  struct Unit();

  func foo(Unit x ; Unit) {
    x;
  };

  func foo(Unit x, Unit y; Unit) {
    y;
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg

