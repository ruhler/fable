
# Two unions with different names are considered different types, even if
# they have the same fields.

set prg {
  struct Unit();

  union A(Unit x, Unit y);
  union B(Unit x, Unit y);

  func main( ; A) {
    B:x(Unit());
  };
}

expect_malformed $prg main
expect_malformed_b $prg 3
