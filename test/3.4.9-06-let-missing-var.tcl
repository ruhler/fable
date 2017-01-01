set prg {
  # Variable declarations need both a type and variable name.
  struct Unit();
  struct A(Unit x, Unit y);

  func main( ; A) {
    # The variable name is missing.
    Unit = Unit();
    A(v, v);
  };
}
# TODO: The error could be at the 'Unit' or the '='?
fblc-check-error $prg 8:5
