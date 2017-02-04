set prg {
  # Test the most basic 'expect_malformed' test.
  struct Unit();

  X X X X X X X 

  func main( ; Unit) {
    Unit();
  };
}

fblc-check-error $prg 5:7
