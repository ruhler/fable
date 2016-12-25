
# A struct can be declared that contains no fields.

set prg {
  struct NoFields();

  func main( ; NoFields) {
    NoFields();
  };
}

expect_result NoFields() $prg main
expect_result_b 0 $prg 1

