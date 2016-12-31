set prg {
  struct Unit();

  proc main( ; ; Unit) {
    # The open parenthesis is missing
    $ Unit());
  };
}
fblc-check-error $prg 6:7
