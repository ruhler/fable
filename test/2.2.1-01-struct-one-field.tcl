
# A struct can be declared that contains one field.

set prg {
  struct Unit();
  struct OneField(Unit x);

  func main( ; OneField) {
    OneField(Unit());
  };
}

expect_result OneField(Unit()) $prg main
expect_result_b 0 $prg 2

