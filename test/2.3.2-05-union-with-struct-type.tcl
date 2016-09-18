
set prg {
  struct Unit();
  struct Foo(Unit bar);

  func main( ; Foo) {
    // Foo is a struct type, not a union type.
    Foo:bar(Unit());
  };
}

expect_malformed $prg main

