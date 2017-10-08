set prg {
  # Process port arguments must have a type.
  struct Unit();

  proc p(- px, Unit+ py ; Unit x, Unit y; Unit) {
    $(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg 5:10
