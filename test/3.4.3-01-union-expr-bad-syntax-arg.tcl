set prg {
  // The argument to a union expression must have valid syntax.
  struct Unit();
  struct A(Unit x, Unit y);
  union Foo(Unit bar, A sludge);

  func main( ; Unit) {
    // The argument isn't valid syntax.
    Foo:bar(???).bar;
  };
}
fblc-check-error $prg 9:14
