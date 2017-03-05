set prg {
  struct Unit();

  proc main( Unit ~> out ; ; Unit) {
    # The port name is missing
    ~ (Unit());
  };
}

fblc-check-error $prg 6:7
