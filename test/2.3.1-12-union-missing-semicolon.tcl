set prg {
  # A union declaration must have a final semicolon.
  struct Unit();
  union Foo(Unit x, Unit y)

  func main( ; Unit) {
    Unit();
  };
}
fblc-check-error $prg 6:3
