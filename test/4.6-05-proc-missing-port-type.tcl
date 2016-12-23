
# Process port arguments must have a type.
set prg {
  struct Unit();

  proc p( <~ px, Unit ~> py ; Unit x, Unit y; Unit) {
    $(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg

