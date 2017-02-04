
# A struct can be declared that contains no fields.

set prg {
  struct NoFields();

  func main( ; NoFields) {
    NoFields();
  };
}

expect_result NoFields() $prg main
