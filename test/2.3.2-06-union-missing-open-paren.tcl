set prg {
  struct Unit();
  struct A(Unit x, Unit y);
  union Foo(Unit bar, A sludge);

  func main( ; Foo) {
    # The open parenthesis before the argument is missing.
    Foo:bar ?(Foo:bar(Unit()) ; Unit(), Unit()));
  };
}
fblc-check-error $prg 8:13
