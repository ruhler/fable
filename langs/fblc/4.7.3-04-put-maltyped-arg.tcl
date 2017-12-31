set prg {
  struct Unit();
  struct Donut();

  proc f(Unit+ myput ; ; Unit) {
    # The argument to myput has the wrong type.
    +myput(Donut());
  };
}
fblc-check-error $prg 7:12
