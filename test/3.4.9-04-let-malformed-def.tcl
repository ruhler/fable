

# Variable definitions must not be malformed.

set prg {
  struct Unit();
  struct A(Unit x, Unit y);

  func main( ; A) {
    // The variable 'x' has not been declared.
    Unit v = x;
    A(v, v);
  };
}

expect_malformed $prg main

