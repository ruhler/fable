

# Function declarations must have unique argument names.
set prg {
  struct Unit();

  func f(Unit x, Unit x; Unit) {
    x;
  };

  func main( ; Unit) {
    Unit();
  };
}
expect_malformed $prg main
expect_malformed_b $prg 2

