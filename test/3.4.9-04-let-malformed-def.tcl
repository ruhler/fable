set prg {
  # Variable definitions must not be malformed.
  struct Unit();
  struct A(Unit x, Unit y);

  func main( ; A) {
    # The variable 'x' has not been declared.
    Unit v = x;
    A(v, v);
  };
}
fblc-check-error $prg 8:14
