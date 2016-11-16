
# The argument to a union expression must have valid syntax.
set prg {
  struct Unit();
  struct A(Unit x, Unit y);
  union Foo(Unit bar, A sludge);

  func main( ; Unit) {
    // The argument isn't valid syntax.
    Foo:bar(???).bar;
  };
}

expect_malformed $prg main
