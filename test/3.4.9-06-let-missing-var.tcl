
# Variable declarations need both a type and variable name.

set prg {
  struct Unit();
  struct A(Unit x, Unit y);

  func main( ; A) {
    // The variable name is missing.
    Unit = Unit();
    A(v, v);
  };
}

expect_malformed $prg main

