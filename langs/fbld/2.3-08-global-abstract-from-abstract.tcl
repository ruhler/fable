set prg {
  struct Unit();

  abst struct Pair(Unit first, Unit second);

  func foo(Pair x; Unit) {
    # The 'first' field of Pair can be accessed, because Pair is defined in
    # this module.
    x.first;
  };

  func main( ; Unit) {
    foo(Pair(Unit(), Unit()));
  };
}

fbld-test $prg "main" {} {
  return Unit()
}
