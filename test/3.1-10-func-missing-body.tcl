

# Function declarations must have a body.
set prg {
  struct Unit();

  func f(Unit x, Unit y; Unit);

  func main( ; Unit) {
    Unit();
  };
}
fblc-check-error $prg

