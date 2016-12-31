set prg {
  # The argument to a union expression must be well formed.
  struct Unit();
  struct A(Unit x, Unit y);
  union Foo(Unit bar, A sludge);

  func main( ; Unit) {
    # The variable 'x' has not  been declared.
    Foo:bar(x).bar;
  };
}
fblc-check-error $prg 9:13
