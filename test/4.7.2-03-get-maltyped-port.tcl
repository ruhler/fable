
set prg {
  struct Unit();
  struct Donut();

  proc f(Donut <~ myget ; ; Unit) {
    // myget port has the wrong type.
    myget~();
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}

expect_malformed $prg main

