
# Process declarations must have an open parenthesis.
set prg {
  struct Unit();

  proc p Unit <~ px, Unit ~> py ; Unit x, Unit y; Unit) {
    $(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
expect_malformed $prg main

