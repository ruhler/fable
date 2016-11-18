
set prg {
  struct Unit();

  func main( ; Unit) {
    Unit();
  ; // Missing the '}' brace.
}

expect_malformed $prg main

