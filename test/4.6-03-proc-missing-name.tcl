
# Process declarations must have a name.
set prg {
  struct Unit();

  proc ( ; ; Unit) {
    $(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
expect_malformed $prg main
expect_malformed_b $prg 2

