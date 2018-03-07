set prg {
  struct Unit();
  union Maybe<type T>(T just, Unit nothing);

  interf Foo {
    import @ { Unit; };
    struct Bar(Unit a, Unit b);
  };

  func main( ; Unit) {
    # Foo is an interface, but a type is expected.
    Maybe<Foo>:nothing(Unit()).nothing;
  };
}

fbld-check-error $prg 12:11
