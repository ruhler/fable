set prg {
  # We can rename entities imported from children.
  import M { Donut=Unit; MkUnit; };

  module M {
    struct Unit();

    func MkUnit( ; Unit) {
      Unit();
    };
  };

  func main( ; Donut) {
    MkUnit();
  };
}

fbld-test $prg "main" {} {
  return Unit@M()
}
