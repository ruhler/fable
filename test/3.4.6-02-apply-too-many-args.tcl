
# Test for calling a function with too many arguments.

set prg {
  struct Unit();

  func f(Unit x; Unit) {
    x;
  };

  func main( ; Unit) {
    f(Unit(), Unit());
  };
}

fblc-check-error $prg
