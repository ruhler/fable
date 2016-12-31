set prg {
  // The object of a field access must be well formed.
  struct Unit();
  struct A(Unit x, Unit y);

  func main( ; Unit) {
    // The variable 'x' has not  been declared.
    x.y;
  };
}
fblc-check-error $prg 8:5
