set prg {
  struct Unit();

  proc main( ; ; Unit) {
    // Missing a semicolon.
    $(Unit())
  };
}

fblc-check-error $prg

