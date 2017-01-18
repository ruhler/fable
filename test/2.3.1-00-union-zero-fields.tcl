set prg {
  # A union must have at least one field.
  struct Unit();
  union NoFields();

  func main( ; Unit) {
    Unit();
  };
}
# TODO: Should the error be at the NoFields, the open paren, or the close
# paren?
fblc-check-error $prg 4:18
