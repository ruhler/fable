

# A function and process can't have the same name.
set prg {
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
expect_malformed $prg main

