set prg {
  # Test a function with no inputs arguments.
  struct Unit();

  func main( ; Unit) {
    Unit();
  };
}

fblc-test $prg main {} "return Unit()"
