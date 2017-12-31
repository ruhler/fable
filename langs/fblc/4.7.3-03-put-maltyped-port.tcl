set prg {
  struct Unit();
  struct Donut();

  proc f(Donut+ myput ; ; Unit) {
    # myput port has the wrong type.
    +myput(Donut());
  };
}
fblc-check-error $prg 7:5
