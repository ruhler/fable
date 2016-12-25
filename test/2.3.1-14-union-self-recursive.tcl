
# A union can have a field with its same type.
set prg {
  struct Unit();
  union Recursive(Unit x, Recursive y);

  func main( ; Recursive) {
    Recursive:y(Recursive:x(Unit()));
  };
}
expect_result Recursive:y(Recursive:x(Unit())) $prg main
expect_result_b 10 $prg 2

