

# Function declarations must have a return type
set prg {
  struct Unit();

  func f(Unit x, Unit y; ) {
    x;
  };

  func main( ; Unit) {
    Unit();
  };
}
expect_malformed $prg main

