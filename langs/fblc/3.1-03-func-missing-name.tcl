set prg {
  # Declared functions must have a name.
  struct Unit();

  func (Unit x, Unit y; Unit) {
    x;
  };
}
fblc-check-error $prg 5:8
