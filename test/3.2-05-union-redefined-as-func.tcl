

# A union and a function can't have the same name.
set prg {
  struct Unit();
  union A(Unit x, Unit y);

  func A(Unit a, Unit b, Unit c ; Unit) {
    Unit();
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
expect_malformed $prg main

