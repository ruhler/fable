set prg {

  module StdLib {
    struct Unit();
    abst struct Foo(Unit a, Unit b);

    func mkFoo(Unit a, Unit b; Foo) {
      Foo(a, b);
    };

    func aFoo(Foo x ; Unit) {
      x.a;
    };
  };

  # The Foo type can be referred to and manipulated via mkFoo and aFoo, even
  # though it is marked as abstract.
  func main( ; Unit@StdLib) {
    aFoo@StdLib(mkFoo@StdLib(Unit@StdLib(), Unit@StdLib()));
  };
}

fbld-test $prg "main" {} {
  return Unit@StdLib()
}
