set prg {
  # Test the most basic 'fblc-test' test.
  struct Unit();

  func main( ; Unit) {
    Unit();
  };
}

fblc-test $prg main {} "return Unit()"
