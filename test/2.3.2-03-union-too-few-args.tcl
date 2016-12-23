
set prg {
  struct Unit();
  union Foo(Unit bar);

  func main( ; Foo) {
    // The constructor is missing its argument.
    Foo:bar();
  };
}

fblc-check-error $prg

