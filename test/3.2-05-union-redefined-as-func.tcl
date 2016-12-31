set prg {
  # A union and a function can't have the same name.
  struct Unit();
  union A(Unit x, Unit y);

  func A(Unit a, Unit b, Unit c ; Unit) {
    Unit();
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg 6:8
