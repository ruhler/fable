set prg {
  struct Unit();
  struct Donut();

  proc f(Donut- myget ; ; Unit) {
    # myget port has the wrong type.
    -myget();
  };
}
fblc-check-error $prg 7:5
