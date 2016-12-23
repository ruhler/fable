
set prg {
  struct Unit();

  proc f(Unit ~> myput ; ; Unit) {
    // Too many arguments to put.
    myput~(Unit(), Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}

fblc-check-error $prg

