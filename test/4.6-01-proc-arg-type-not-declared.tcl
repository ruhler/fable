
# Argument types of a process must be defined.
set prg {
  struct Unit();

  proc f( ; Unit x, Donut y; Unit) {
    $(x);
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
expect_malformed $prg main

