set prg {
  struct Unit();
  union Maybe<type T>(T just, Unit nothing);

  func Foo( ; Unit) {
    Unit();
  };

  func main( ; Unit) {
    # Foo is a function, but a type is expected.
    Maybe<Foo>:nothing(Unit()).nothing;
  };
}

fbld-check-error $prg 11:11
