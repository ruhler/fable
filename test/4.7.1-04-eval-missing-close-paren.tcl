set prg {
  struct Unit();

  proc main( ; ; Unit) {
    // The close parenthesis is missing.
    $(Unit();
  };
}
fblc-check-error $prg 6:13
