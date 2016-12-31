set prg {
  # Accessing an unknown field of a struct.
  struct Unit();
  struct A(Unit x, Unit y);

  func main( ; Unit) {
    .z(A(Unit(), Unit()));
  };
}
fblc-check-error $prg 7:6
