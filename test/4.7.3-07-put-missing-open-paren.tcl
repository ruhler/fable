set prg {
  struct Unit();

  proc f(Unit ~> myput ; ; Unit) {
    // The open parenthesis is missing.
    myput~ Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg 6:12
