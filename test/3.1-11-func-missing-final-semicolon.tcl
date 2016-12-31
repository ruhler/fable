set prg {
  // Function declarations must have a final semicolon.
  struct Unit();

  func f(Unit x, Unit y; Unit) {
    x;
  }

  func main( ; Unit) {
    Unit();
  };
}
fblc-check-error $prg 9:3
