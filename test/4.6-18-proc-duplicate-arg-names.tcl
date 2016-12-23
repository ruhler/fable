

# Process arg names must be unique.
set prg {
  struct Unit();

  proc p(Unit <~ px, Unit ~> py ; Unit x, Unit x ; Unit) {
    $(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg

