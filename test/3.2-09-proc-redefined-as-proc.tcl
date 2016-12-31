set prg {
  # Two processes can't have the same name.
  struct Unit();

  proc foo( ; Unit x ; Unit) {
    $(x);
  };

  proc foo( ; Unit x, Unit y; Unit) {
    $(y);
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg 9:8
