

# Function declarations must have a semicolon separating inputs from outputs.
set prg {
  struct Unit();

  func f(Unit x, Unit y Unit) {
    x;
  };

  func main( ; Unit) {
    Unit();
  };
}
expect_malformed $prg main
expect_malformed_b $prg 2

