set prg {
  # Process arguments must be terminated with a semicolon.
  struct Unit();

  proc p(Unit <~ px, Unit ~> py ; Unit x, Unit y Unit) {
    $(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
} 
fblc-check-error $prg 5:50
