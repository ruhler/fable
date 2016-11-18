
set prg {
  struct Unit();

  proc f(Unit ~> myget ; ; Unit) {
    // myget port is a put port, not a get port.
    myget~();
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}

expect_malformed $prg main

