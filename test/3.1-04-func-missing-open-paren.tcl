

# Function declarations must have an open parenthesis.
set prg {
  struct Unit();

  func f Unit x, Unit y; Unit) {
    x;
  };

  func main( ; Unit) {
    Unit();
  };
}
fblc-check-error $prg

