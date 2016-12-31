set prg {
  // Accessing an unknown field of a struct.
  struct Unit();
  struct A(Unit x, Unit y);

  func main( ; Unit) {
    A(Unit(), Unit()).z;
  };
}
fblc-check-error $prg 7:23
