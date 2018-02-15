set prg {
  interf Id<type T> {
    import @ { T; };
    func id(T x; T);
  };

  # Static parameters are not visible to previous static parameters in the list.
  func idf<module M(Id<A>), type A>(A x; A) {
    id@M(x);
  };
}

skip fbld-check-error $prg 8:24
