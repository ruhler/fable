
# Argument types of a function must be defined.
set prg {
  struct Unit();

  func f(Unit x, Donut y; Unit) {
    x;
  };

  func main( ; Unit) {
    Unit();
  };
}
expect_malformed $prg main

