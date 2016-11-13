
# 'foo' is not a valid kind of declaration.
set prg {
  struct Unit();

  foo Bar();

  func main( ; Unit) {
    Unit();
  };
}
expect_malformed $prg main

