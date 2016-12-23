
set prg {
  struct Unit();

  proc f(Unit <~ myput ; ; Unit) {
    // myput port has the wrong polarity.
    myput~(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}

fblc-check-error $prg

