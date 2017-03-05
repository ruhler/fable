set prg {
  # A struct must be constructed with sufficient number of arguments.
  struct Unit();
  struct Donut();
  struct A(Unit x, Donut y);

  func main( ; A) {
    # There should be two arguments provided, not one.
    A(Unit());
  };
}

# TODO: Should the error location be at the 'A', the open paren, or the close
# paren?
fblc-check-error $prg 9:5
