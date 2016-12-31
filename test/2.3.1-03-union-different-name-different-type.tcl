set prg {
  # Two unions with different names are considered different types, even if
  # they have the same fields.
  struct Unit();

  union A(Unit x, Unit y);
  union B(Unit x, Unit y);

  func main( ; A) {
    B:x(Unit());
  };
}
fblc-check-error $prg 10:5
