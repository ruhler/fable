
# The argument to a union expression must be well formed.
set prg {
  struct Unit();
  struct A(Unit x, Unit y);
  union Foo(Unit bar, A sludge);

  func main( ; Unit) {
    // The variable 'x' has not  been declared.
    Foo:bar(x).bar;
  };
}

fblc-check-error $prg
