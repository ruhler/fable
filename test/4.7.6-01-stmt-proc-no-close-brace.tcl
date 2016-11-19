set prg {
  struct Unit();

  proc main( ; ; Unit) {
    $(Unit());
  ; // Missing the '}' brace.
}

expect_malformed $prg main

