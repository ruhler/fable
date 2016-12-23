
# Test for calling a function with too few arguments.

set prg {
  struct Unit();

  func f(Unit x; Unit) {
    x;
  };

  func main( ; Unit) {
    f();
  };
}

fblc-check-error $prg
