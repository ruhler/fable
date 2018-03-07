set prg {
  struct Unit();

  func f<struct S(Unit a, Unit b)>( ; Unit) {
    S(Unit(), Unit()).b;
  };

  union Foo(Unit a, Unit b);

  func main( ; Unit) {
    # Foo is a union type, but a struct type is required.
    f<Foo>();
  };
}

fbld-check-error $prg 12:7
