set prg {
  struct Unit();

  func f(Unit x; Unit) {
    x;
  };

  func main( ; Unit) {
    // The argument has invalid syntax.
    f(???);
  };
}
fblc-check-error $prg 10:8
