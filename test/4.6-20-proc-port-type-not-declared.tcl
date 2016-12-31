set prg {
  struct Unit();

  # The process port type Donut is not declared
  proc p(Donut <~ px, Unit ~> py ; Unit x, Unit y ; Unit) {
    $(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg 5:10
