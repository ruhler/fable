set prg {
  struct Unit();

  proc f(Unit ~> myput ; ; Unit) {
    # The close parenthesis is missing.
    myput~(Unit();
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg 6:18
