set prg {
  struct Unit();
  struct Donut();

  proc f(Donut ~> myput ; ; Unit) {
    # myput port has the wrong type.
    ~myput(Donut());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
fblc-check-error $prg 7:6
