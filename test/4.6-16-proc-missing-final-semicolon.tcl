
# Process declarations must have a final semicolon.
set prg {
  struct Unit();

  proc p(Unit <~ px, Unit ~> py ; Unit x, Unit y ; Unit) {
    $(Unit());
  }

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg

