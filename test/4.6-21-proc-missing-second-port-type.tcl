# All process port arguments must have a type.
set prg {
  struct Unit();

  // The second port argument is missing its type.
  proc p(Unit <~ px, ~> py ; Unit x, Unit y; Unit) {
    $(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg

