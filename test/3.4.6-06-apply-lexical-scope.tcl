set prg {
  # Test that variables in the calling scope are not visible to the function.
  struct Unit();

  func f(Unit x ; Unit) {
    y;
  };

  func main( ; Unit) {
    Unit y = Unit();
    f(Unit());
  };
}
fblc-check-error $prg 6:5
