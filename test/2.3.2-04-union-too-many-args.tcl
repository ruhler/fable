set prg {
  struct Unit();
  union Foo(Unit bar);

  func main( ; Foo) {
    // The constructor has too many arguments.
    Foo:bar(Unit(), Unit());
  };
}
# TODO: Where exactly should the error be?
fblc-check-error $prg 7:19
