set prg {
  struct Unit();
  struct Donut();

  struct Pair<struct S<type A>(A x), type B>(B first, S<Unit> second);

  struct MyS<type A>(A x);

  func main( ; Unit) {
    # Check that we can use a polymorphic static parameter arg.
    Pair<MyS, Donut>(Donut(), MyS<Unit>(Unit())).second.x;
  };
}

fbld-test $prg "main" {} {
  return Unit()
}
