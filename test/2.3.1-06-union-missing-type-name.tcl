
# A union declaration must have a name.
set prg {
  struct Unit();
  union (Unit x, Unit y);

  func main( ; Unit) {
    Unit();
  };
}
fblc-check-error $prg
