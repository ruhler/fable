
set prg {
  struct Unit();

  func main( ; Unit) {
    // Missing a semicolon.
    Unit()
  };
}

fblc-check-error $prg

