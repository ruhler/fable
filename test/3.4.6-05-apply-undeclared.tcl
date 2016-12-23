
# Test for calling a function that has not been declared.

set prg {
  struct Unit();

  func main( ; Unit) {
    f(Unit());
  };
}

fblc-check-error $prg

