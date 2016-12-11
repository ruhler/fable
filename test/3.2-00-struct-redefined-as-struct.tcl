

# A struct must not be declared multiple times.
set prg {
  struct Unit();
  struct A(Unit x, Unit y);
  struct A(Unit a, Unit b, Unit c);

  proc main( ; ; Unit) {
    $(Unit());
  };
}
expect_malformed $prg main
expect_malformed_b $prg 3

