set prg {
  struct Unit();
  union Maybe<type T>(T just, Unit nothing);

  func main<type Foo>( ; Unit) {
    # Foo is a type, as required
    Maybe<Foo>:nothing(Unit()).nothing;
  };
}

fbld-test $prg "main<Unit>" {} {
  return Unit()
}
