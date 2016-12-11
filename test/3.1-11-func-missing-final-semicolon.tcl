

# Function declarations must have a final semicolon.
set prg {
  struct Unit();

  func f(Unit x, Unit y; Unit) {
    x;
  }

  func main( ; Unit) {
    Unit();
  };
}
expect_malformed $prg main
expect_malformed_b $prg 2

