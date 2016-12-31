set prg {
  // Process arguments must have a type.
  struct Unit();

  proc p(Unit <~ px, Unit ~> py ; x, Unit y; Unit) {
    $(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg 5:36
