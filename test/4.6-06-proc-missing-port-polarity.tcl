set prg {
  // Process port arguments must have a polarity.
  struct Unit();

  proc p(Unit px, Unit ~> py ; Unit x, Unit y; Unit) {
    $(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg 5:15
