set prg {
  struct Unit();
  struct A(Unit x, Unit y);

  func main( ; A) {
    # The equals sign is missing.
    Unit v Unit();
    A(v, v);
  };
}
fblc-check-error $prg 7:12
