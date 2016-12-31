set prg {
  struct Unit();

  proc main( ; ; Unit) {
    $(Unit());
  ; // Missing the '}' brace.
}
fblc-check-error $prg 6:3
