set prg {
  # Function declarations must have commas separating arguments.
  struct Unit();

  func f(Unit x Unit y; Unit) {
    x;
  };

  func main( ; Unit) {
    Unit();
  };
}
fblc-check-error $prg 5:17
