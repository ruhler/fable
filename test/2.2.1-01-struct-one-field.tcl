set prg {
  # A struct can be declared that contains one field.
  struct Unit();
  struct OneField(Unit x);

  func main( ; OneField) {
    OneField(Unit());
  };
}

expect_result OneField(Unit()) $prg main
