set prg {
  struct Unit();

  struct P<type T>(T head, S<T> tail);
  union S<type T>(P<T> cons, Unit nil); 

  # Regression test: Use of the function F depends on the type variable A
  # declared earlier in the parameter list.
  func MapS<type A, func F(A x; A)>(S<A> a; S<A>) {
    ?(a; cons: S<A>:cons(MapP<A, F>(a.cons)), nil: S<A>:nil(Unit()));
  };

  func MapP<type A, func F(A x; A)>(P<A> a; P<A>) {
    P<A>(F(a.head), MapS<A, F>(a.tail));
  };

  func IdUnit(Unit x; Unit) {
    x;
  };

  func main( ; Unit) {
    MapP<Unit, IdUnit>(P<Unit>(Unit(), S<Unit>:nil(Unit()))).head;
  };
}

fbld-test $prg "main" {} {
  return Unit()
}
