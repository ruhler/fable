
set prg {
  struct Unit();
  struct A(Unit x, Unit y);

  func main( ; A) {
    // The let argument has a syntax error.
    Unit v = ???;
    A(v, v);
  };
}

fblc-check-error $prg

