set prg {
  // 'foo' is not a valid kind of declaration.
  struct Unit();

  foo Bar();

  func main( ; Unit) {
    Unit();
  };
}
fblc-check-error $prg 5:3
