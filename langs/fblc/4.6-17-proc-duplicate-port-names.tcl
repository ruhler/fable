set prg {
  # Process port names must be unique.
  struct Unit();

  proc p(Unit- px, Unit+ px ; Unit x, Unit y ; Unit) {
    $(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg 5:26
