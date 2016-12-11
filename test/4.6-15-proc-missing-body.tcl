
# Process declarations must have a body.
set prg {
  struct Unit();

  proc p(Unit <~ px, Unit ~> py ; Unit x, Unit y ; Unit);

  proc main( ; ; Unit) {
    $(Unit());
  };
}
expect_malformed $prg main
expect_malformed_b $prg 2

