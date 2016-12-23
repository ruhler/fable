
# A struct declaration must have a name.
set prg {
  struct Unit();
  struct (Unit x, Unit y);

  func main( ; Unit) {
    Unit();
  };
}
fblc-check-error $prg
