
set prg {
  struct Unit();
  struct A(Unit x, Unit y);

  func main( ; A) {
    // The body of the let is missing.
    Unit v = Unit();
  };
}

expect_malformed $prg main

