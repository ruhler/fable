

# Process port names must be unique.
set prg {
  struct Unit();

  proc p(Unit <~ px, Unit ~> px ; Unit x, Unit y ; Unit) {
    $(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
expect_malformed $prg main
expect_malformed_b $prg 2

