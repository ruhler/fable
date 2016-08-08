
# Each field of a struct declaration needs a name.
set prg {
  struct Unit();
  struct Foo(Unit x, Unit, Unit z);

  func main( ; Foo) {
    Foo(Unit(), Unit(), Unit());
  };
}
expect_malformed $prg main

