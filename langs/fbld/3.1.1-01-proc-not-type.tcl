set prg {
  struct Unit();
  union Maybe<type T>(T just, Unit nothing);

  proc Foo( ; ; Unit) {
    $(Unit());
  };

  func main( ; Unit) {
    # Foo is a proc, but a type is expected.
    Maybe<Foo>:nothing(Unit()).nothing;
  };
}

fbld-check-error $prg 11:11
