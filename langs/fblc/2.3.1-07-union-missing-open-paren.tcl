set prg {
  # A union declaration must have an open paren.
  struct Unit();
  union Foo Unit x, Unit y);

  func main( ; Unit) {
    Unit();
  };
}
fblc-check-error $prg 4:13
