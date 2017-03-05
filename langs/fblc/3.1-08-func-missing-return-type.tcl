set prg {
  # Function declarations must have a return type
  struct Unit();

  func f(Unit x, Unit y; ) {
    x;
  };

  func main( ; Unit) {
    Unit();
  };
}
fblc-check-error $prg 5:26
