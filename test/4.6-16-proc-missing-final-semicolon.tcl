set prg {
  // Process declarations must have a final semicolon.
  struct Unit();

  proc p(Unit <~ px, Unit ~> py ; Unit x, Unit y ; Unit) {
    $(Unit());
  }

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg 9:3
