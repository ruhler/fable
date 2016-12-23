
# A union must have at least one field.

set prg {
  struct Unit();
  union NoFields();

  func main( ; Unit) {
    Unit();
  };
}

fblc-check-error $prg

