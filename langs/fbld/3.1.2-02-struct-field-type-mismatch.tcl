set prg {
  struct Unit();
  struct Donut();

  func f<struct S(Unit a, Unit b)>( ; Unit) {
    S(Unit(), Unit()).b;
  };

  struct Foo(Unit a, Donut b);

  func main( ; Unit) {
    # Field b of Foo has the wrong type.
    f<Foo>();
  };
}

fbld-check-error $prg 13:7
