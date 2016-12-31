set prg {
  // Test for calling a function with too many arguments.
  struct Unit();

  func f(Unit x; Unit) {
    x;
  };

  func main( ; Unit) {
    f(Unit(), Unit());
  };
}
fblc-check-error $prg 10:5
