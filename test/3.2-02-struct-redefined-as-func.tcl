

# A struct and a function can't have the same name.
set prg {
  struct Unit();
  struct A(Unit x, Unit y);

  func A(Unit a, Unit b, Unit c ; Unit) {
    Unit();
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg

