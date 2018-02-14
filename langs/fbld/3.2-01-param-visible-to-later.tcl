set prg {
  interf Id<type T> {
    import @ { T; };
    func id(T x; T);
  };

  # Static parameters are visible to later static parameters in the list.
  func idf<type A, module M(Id<A>)>(A x; A) {
    id@M(x);
  };

  struct Unit();
  module IdUnit(Id<Unit>) {
    import @ { Unit; };
    func id(Unit x; Unit) {
      x;
    };
  };

  func main( ; Unit) {
    idf<Unit, IdUnit>(Unit());
  };
}

fbld-test $prg "main" {} {
  return Unit()
}
