
# A union can be declared that contains one field.

set prg {
  struct Unit();
  union OneField(Unit x);

  func main( ; OneField) {
    OneField:x(Unit());
  };
}

expect_result OneField:x(Unit()) $prg main
expect_result_b 0 $prg 2

