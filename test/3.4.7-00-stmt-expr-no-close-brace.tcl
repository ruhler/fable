set prg {
  struct Unit();

  func main( ; Unit) {
    Unit();
  ; // Missing the '}' brace.
}
fblc-check-error $prg 6:3
