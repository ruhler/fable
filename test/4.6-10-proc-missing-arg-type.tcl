
# Process arguments must have a type.
set prg {
  struct Unit();

  proc p(Unit <~ px, Unit ~> py ; x, Unit y; Unit) {
    $(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
expect_malformed $prg main

