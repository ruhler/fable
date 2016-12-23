
# A struct declaration must have an open paren.
set prg {
  struct Unit();
  struct Foo Unit x, Donut y);

  func main( ; Unit) {
    Unit();
  };
}
fblc-check-error $prg
