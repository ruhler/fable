set prg {
  # A struct can be declared that contains no fields.
  struct NoFields();

  func main( ; NoFields) {
    NoFields();
  };
}

fblc-test $prg main {} "return NoFields()"
