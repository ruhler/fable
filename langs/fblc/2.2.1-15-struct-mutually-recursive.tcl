set prg {
  # structs can be mutually recursive.
  struct Unit();
  struct Foo(Bar x);
  struct Bar(Foo y);

  func main( ; Unit) {
    Unit();
  };
}

fblc-test $prg main {} "return Unit()"
