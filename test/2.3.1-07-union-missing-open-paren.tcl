set prg {
  // A union declaration must have an open paren.
  struct Unit();
  union Foo Unit x, Donut y);

  func main( ; Unit) {
    Unit();
  };
}
fblc-check-error $prg 4:13
