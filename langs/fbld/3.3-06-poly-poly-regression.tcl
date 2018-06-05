set prg {
  struct Unit();

  struct P<type T>(T head, S<T> tail);
  union S<type T>(P<T> cons, Unit nil); 

  func ConsS<type T>(T a, S<T> l; S<T>) S<T>:cons(P<T>(a, l));
  func S0<type T>( ; S<T>) S<T>:nil(Unit());
  func S1<type T>(T a; S<T>) ConsS<T>(a, S0<T>());

  func Append<type T>(S<T> a, S<T> b ; S<T>) {
    ?(a; cons: S<T>:cons(P<T>(a.cons.head, Append<T>(a.cons.tail, b))), nil: b);
  };

  func Concat<type T>(S<S<T>> x; S<T>) {
    # Regression test for a bug where this would give an error:
    # x.cons.head: expected type S<T>, but got type S
    # x.cons.tail: expected type S<S<T>>, but got type S<S>
    ?(x; cons: Append<T>(x.cons.head, Concat<T>(x.cons.tail)), nil: S0<T>());
  };

  func main( ; Unit) {
    Concat<Unit>(S1<S<Unit>>(S1<Unit>(Unit()))).cons.head;
  };
}

fbld-test $prg "main" {} {
  return Unit()
}
