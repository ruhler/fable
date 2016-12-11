

# A union must not be declared multiple times.
set prg {
  struct Unit();
  union A(Unit x, Unit y);
  union A(Unit a, Unit b, Unit c);

  proc main( ; ; Unit) {
    $(Unit());
  };
}
expect_malformed $prg main
expect_malformed_b $prg 3

