
set prg {
  struct Unit();
  struct A(Unit x, Unit y);

  func main( ; A) {
    // The variable definition is missing.
    Unit v = ;
    A(v, v);
  };
}

expect_malformed $prg main
expect_malformed_b $prg 2

