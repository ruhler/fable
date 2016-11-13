

# Function declarations must have types for each argument.
set prg {
  struct Unit();

  func f(x, Unit y; Unit) {
    x;
  };

  func main( ; Unit) {
    Unit();
  };
}
expect_malformed $prg main

