
set prg {
  struct Unit();
  union Foo(Unit bar);

  func main( ; Foo) {
    // The constructor has too many arguments.
    Foo:bar(Unit(), Unit());
  };
}

fblc-check-error $prg

