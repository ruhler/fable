set prg {
  # Test a simple eval process.
  struct Unit();

  proc main( ; ; Unit) {
    $(Unit());
  };
}

fblc-test $prg main {} "return Unit()"
