set prg {
  struct Unit();
  union Maybe<type T>(T just, Unit nothing);

  module Foo {
    import @ { Unit; };
    struct Blah(Unit a);
  };

  func main( ; Unit) {
    # Foo is a module, not a type as required
    Maybe<Foo>:nothing(Unit()).nothing;
  };
}

fbld-check-error $prg 12:11
