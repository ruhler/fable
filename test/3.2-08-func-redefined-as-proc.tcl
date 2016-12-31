set prg {
  // A function and process can't have the same name.
  struct Unit();

  func foo(Unit x ; Unit) {
    x;
  };

  proc foo( ; Unit x, Unit y; Unit) {
    $(y);
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg 9:8
