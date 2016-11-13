
# Process port arguments must be separated with a comma.
set prg {
  struct Unit();

  proc p(Unit <~ px Unit ~> py ; Unit x, Unit y; Unit) {
    $(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
expect_malformed $prg main

