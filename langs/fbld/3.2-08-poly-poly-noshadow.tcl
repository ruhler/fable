set prg {
  struct Unit();

  # Can reuse the same name in a nest static parameter if the names don't
  # lead to shadowing.
  struct Pair<struct S<type A>(A x), type A>(A first, S<Unit> second);

  func main( ; Unit) {
    Unit();
  };
}

fbld-test $prg "main" {} {
  return Unit()
}
