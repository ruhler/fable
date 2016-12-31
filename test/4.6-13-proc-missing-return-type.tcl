set prg {
  // Process declarations must have a return type.
  struct Unit();

  proc p(Unit <~ px, Unit ~> py ; Unit x, Unit y ; ) {
    $(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg 5:52
