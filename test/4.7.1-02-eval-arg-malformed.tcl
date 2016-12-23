set prg {
  struct Unit();

  proc main( ; ; Unit) {
    // The argument to eval has the wrong syntax.
    $(???);
  };
}

fblc-check-error $prg

