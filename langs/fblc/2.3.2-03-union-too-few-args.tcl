set prg {
  struct Unit();
  union Foo(Unit bar);

  func main( ; Foo) {
    # The constructor is missing its argument.
    Foo:bar();
  };
}
# TODO: Where exactly should the error location be?
fblc-check-error $prg 7:13
