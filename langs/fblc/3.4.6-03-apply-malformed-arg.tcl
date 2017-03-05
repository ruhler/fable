set prg {
  # Test for calling a function with a malformed argument.
  struct Unit();

  func f(Unit x; Unit) {
    x;
  };

  func main( ; Unit) {
    # The variable 'x' has not been declared.
    f(x);
  };
}
fblc-check-error $prg 11:7
