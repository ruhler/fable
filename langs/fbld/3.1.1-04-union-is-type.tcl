set prg {
  struct Unit();
  union Maybe<type T>(T just, Unit nothing);

  union Foo(Unit a, Unit b);

  func main( ; Unit) {
    # Foo is a union, which is a type as required.
    Maybe<Foo>:nothing(Unit()).nothing;
  };
}

fbld-test $prg main {} {
  return Unit()
}
