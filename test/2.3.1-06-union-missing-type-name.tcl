set prg {
  # A union declaration must have a name.
  struct Unit();
  union (Unit x, Unit y);

  func main( ; Unit) {
    Unit();
  };
}
fblc-check-error $prg 4:9
