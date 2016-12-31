set prg {
  # The return type of a process must match the type of the body.
  struct Unit();
  struct Donut();

  proc main( ; ; Unit) {
    $(Donut());
  };
}
# TODO: Where exactly should the error location be?
fblc-check-error $prg 7:7
