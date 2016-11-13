

# All port types must be declared.
set prg {
  struct Unit();

  proc p(Donut <~ px, Unit ~> py ; Unit x, Unit y ; Unit) {
    $(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
expect_malformed $prg main

