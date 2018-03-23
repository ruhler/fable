set prg {
  # structs can be mutually recursive, even if they take static parameters.
  struct Unit();
  struct Foo<struct T(Unit a, Unit b)>(Bar<T> x, T t);
  struct Bar<struct T(Unit a, Unit b)>(Foo<T> y, T t);

  func main( ; Unit) {
    Unit();
  };
}

fblc-test $prg main {} "return Unit()"
