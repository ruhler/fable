set prg {
  # Function declarations must have a final semicolon.
  struct Unit();

  func f(Unit x; Unit) {
    x;
  }

  func main( ; Unit) {
    f();
  };
}
fblc-check-error $prg 9:3
