
set prg {
  struct Unit();
  struct A(Unit x, Unit y);

  func main( ; A) {
    // The semicolon after the declaration is missing.
    Unit v = Unit()
    A(v, v);
  };
}

expect_malformed $prg main

