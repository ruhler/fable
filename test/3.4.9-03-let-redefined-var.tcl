

# Variables cannot be defined multiple times.

set prg {
  struct Unit();
  struct A(Unit x, Unit y);
  struct B(A a, A b);

  func main( ; B) {
    // The variable 'v' is defined twice.
    Unit v = Unit();
    A v = A(v, v);
    B(v, v);
  };
}

expect_malformed $prg main

