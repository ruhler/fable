set prg {
  // Test for calling a function that has not been declared.
  struct Unit();

  func main( ; Unit) {
    f(Unit());
  };
}
fblc-check-error $prg 6:5
