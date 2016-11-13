

# The return type of a process must be declared.
set prg {
  struct Unit();

  proc p(Unit <~ px, Unit ~> py ; Unit x, Unit y ; Donut) {
    $(Donut());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
expect_malformed $prg main

