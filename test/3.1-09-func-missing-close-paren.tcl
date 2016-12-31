set prg {
  # Function declarations must have a close paren.
  struct Unit();

  func f(Unit x, Unit y; Unit {
    x;
  };

  func main( ; Unit) {
    Unit();
  };
}
fblc-check-error $prg 5:31
