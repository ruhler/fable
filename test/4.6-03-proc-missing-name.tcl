
# Process declarations must have a name.
set prg {
  struct Unit();

  proc ( ; ; Unit) {
    $(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg

