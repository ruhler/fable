

# Two processes can't have the same name.
set prg {
  struct Unit();

  proc foo( ; Unit x ; Unit) {
    $(x);
  };

  proc foo( ; Unit x, Unit y; Unit) {
    $(y);
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
expect_malformed $prg main

