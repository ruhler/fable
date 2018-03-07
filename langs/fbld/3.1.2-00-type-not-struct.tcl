set prg {
  struct Unit();

  func f<struct S(Unit a, Unit b)>( ; Unit) {
    S(Unit(), Unit()).b;
  };

  func main<type Foo>( ; Unit) {
    # Foo is an abstract type, but a struct is expected.
    f<Foo>();
  };
}

fbld-check-error $prg 10:7
