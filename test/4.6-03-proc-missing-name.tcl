set prg {
  # Process declarations must have a name.
  struct Unit();

  proc ( ; ; Unit) {
    $(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg 5:8
