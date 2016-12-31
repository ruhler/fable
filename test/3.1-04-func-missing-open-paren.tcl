set prg {
  # Function declarations must have an open parenthesis.
  struct Unit();

  func f Unit x, Unit y; Unit) {
    x;
  };

  func main( ; Unit) {
    Unit();
  };
}
fblc-check-error $prg 5:10
